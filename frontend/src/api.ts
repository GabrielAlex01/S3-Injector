export interface DeviceStatus {
  status: string;
  uptime_s: number;
  heap_free: number;
  clients: number;
  loot_count: number;
  cdc_connected: boolean;
}

export interface LootEntry {
  id: number;
  timestamp: string;
  type: string;
  data: string;
}

export interface BuiltinPayload {
  id: string;
  label: string;
  description: string;
  risk: string;
}

export interface CustomPayload {
  id: number;
  title: string;
}

export interface PayloadList {
  builtin: BuiltinPayload[];
  custom: CustomPayload[];
}

const BASE = "";
const TOKEN_KEY = "s3_token";

export function getToken(): string | null {
  return sessionStorage.getItem(TOKEN_KEY);
}

export function setToken(t: string) {
  sessionStorage.setItem(TOKEN_KEY, t);
}

export function clearToken() {
  sessionStorage.removeItem(TOKEN_KEY);
}

function authHeaders(): Record<string, string> {
  const t = getToken();
  return t ? { Authorization: `Bearer ${t}` } : {};
}

async function authFetch(url: string, opts: RequestInit = {}): Promise<Response> {
  const headers = { ...authHeaders(), ...(opts.headers as Record<string, string> || {}) };
  const r = await fetch(url, { ...opts, headers });
  if (r.status === 401) {
    clearToken();
    window.location.reload();
    throw new Error("unauthorized");
  }
  return r;
}

export async function login(user: string, pass: string): Promise<string> {
  const r = await fetch(`${BASE}/api/login`, {
    method: "POST",
    headers: { "Content-Type": "application/json" },
    body: JSON.stringify({ user, pass }),
  });
  if (!r.ok) throw new Error("invalid credentials");
  const d = await r.json();
  setToken(d.token);
  return d.token;
}

export async function fetchStatus(): Promise<DeviceStatus> {
  const r = await authFetch(`${BASE}/api/status`);
  return r.json();
}

export async function fetchLoot(): Promise<LootEntry[]> {
  const r = await authFetch(`${BASE}/api/loot`);
  const d = await r.json();
  return d.loot;
}

export async function clearLoot(): Promise<void> {
  await authFetch(`${BASE}/api/loot/clear`, { method: "POST" });
}

export async function executePayload(payloadId: string): Promise<{ queued: boolean }> {
  const r = await authFetch(`${BASE}/api/execute`, {
    method: "POST",
    headers: { "Content-Type": "application/json" },
    body: JSON.stringify({ payload: payloadId }),
  });
  return r.json();
}

export async function fetchPayloads(): Promise<PayloadList> {
  const r = await authFetch(`${BASE}/api/payloads`);
  return r.json();
}

export async function createPayload(title: string, script: string): Promise<{ id: number }> {
  const r = await authFetch(`${BASE}/api/payloads/create`, {
    method: "POST",
    headers: { "Content-Type": "application/json" },
    body: JSON.stringify({ title, script }),
  });
  return r.json();
}

export async function deletePayload(id: number): Promise<void> {
  await authFetch(`${BASE}/api/payloads/delete?id=${id}`, { method: "POST" });
}

export async function testLoot(): Promise<void> {
  await authFetch(`${BASE}/api/loot/test`, { method: "POST" });
}
