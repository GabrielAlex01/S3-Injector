import { useEffect, useState, type ReactNode } from "react";
import { fetchLoot, clearLoot, testLoot, type LootEntry } from "../api";

function esc(s: string): string {
  return s.replace(/&/g, "&amp;").replace(/</g, "&lt;").replace(/>/g, "&gt;");
}

function toArr(v: any): any[] {
  if (Array.isArray(v)) return v;
  if (v == null) return [];
  return [v];
}

function renderLoot(raw: string): ReactNode | null {
  let data: any;
  try {
    data = JSON.parse(raw);
  } catch {
    return null;
  }

  // WiFi profiles: [{s, k}, ...]
  if (Array.isArray(data) && data.length > 0 && data[0]?.s !== undefined) {
    return (
      <div className="loot-formatted">
        <table className="loot-table">
          <thead>
            <tr><th>SSID</th><th>Senha</th></tr>
          </thead>
          <tbody>
            {data.map((w: any, i: number) => (
              <tr key={i}><td>{w.s}</td><td>{w.k || "-"}</td></tr>
            ))}
          </tbody>
        </table>
      </div>
    );
  }

  // Port scan: {gw, listen, scan}
  if (data.gw !== undefined && data.listen) {
    return (
      <div className="loot-formatted">
        <div className="loot-kv">
          <span className="loot-k">Gateway</span><span>{data.gw || "N/A"}</span>
        </div>
        {data.scan?.length > 0 && (
          <div className="loot-kv">
            <span className="loot-k">Abertas no GW</span><span>{data.scan.join(", ")}</span>
          </div>
        )}
        {toArr(data.fw).length > 0 && (
          <>
            <div className="loot-section-title">Regras Allow em Portas Criticas</div>
            <table className="loot-table">
              <thead><tr><th>Regra</th><th>Direcao</th><th>Porta</th></tr></thead>
              <tbody>
                {toArr(data.fw).map((f: any, i: number) => (
                  <tr key={i}>
                    <td>{f.n}</td>
                    <td>{f.d}</td>
                    <td>{f.p || "-"}</td>
                  </tr>
                ))}
              </tbody>
            </table>
          </>
        )}
        {data.listen?.length > 0 && (
          <>
            <div className="loot-section-title">Portas em Escuta</div>
            <table className="loot-table">
              <thead><tr><th>Porta</th><th>Processo</th></tr></thead>
              <tbody>
                {data.listen.map((l: any, i: number) => (
                  <tr key={i}>
                    <td>{l.p ?? l.port ?? "?"}</td>
                    <td>{l.n ?? l.proc ?? "-"}</td>
                  </tr>
                ))}
              </tbody>
            </table>
          </>
        )}
      </div>
    );
  }

  // Defenders: {edr, svc}
  if (data.edr !== undefined || data.svc !== undefined) {
    return (
      <div className="loot-formatted">
        <div className="loot-section-title">Processos EDR/AV</div>
        <pre className="loot-data">
          {Array.isArray(data.edr) ? (data.edr.length > 0 ? data.edr.join(", ") : "Nenhum detectado") : String(data.edr ?? "Nenhum")}
        </pre>
        {data.svc && (
          <>
            <div className="loot-section-title">Servicos de Seguranca</div>
            {Array.isArray(data.svc) ? (
              <table className="loot-table">
                <thead><tr><th>Servico</th><th>Status</th></tr></thead>
                <tbody>
                  {data.svc.map((s: any, i: number) => (
                    <tr key={i}><td>{s.Name}</td><td>{s.Status === 4 ? "Running" : s.Status === 1 ? "Stopped" : String(s.Status)}</td></tr>
                  ))}
                </tbody>
              </table>
            ) : (
              <pre className="loot-data">{JSON.stringify(data.svc, null, 2)}</pre>
            )}
          </>
        )}
      </div>
    );
  }

  // Weaken defenses: {exc, fwIn, fwOut, excList}
  if (data.exc !== undefined && data.fwIn !== undefined) {
    return (
      <div className="loot-formatted">
        <div className="loot-section-title">Diminuir Defesas</div>
        <div className="loot-kv"><span className="loot-k">Exclusao Defender</span><span>{data.exc}</span></div>
        <div className="loot-kv"><span className="loot-k">FW Inbound 4444</span><span>{data.fwIn}</span></div>
        <div className="loot-kv"><span className="loot-k">FW Outbound 4444</span><span>{data.fwOut}</span></div>
        {data.excList && (
          <>
            <div className="loot-section-title">Exclusoes Ativas no Defender</div>
            <pre className="loot-data">{toArr(data.excList).join("\n")}</pre>
          </>
        )}
      </div>
    );
  }

  // Revert defenses: {exc_removed, fwIn_removed, fwOut_removed, dir_removed}
  if (data.exc_removed !== undefined && data.fwIn_removed !== undefined) {
    return (
      <div className="loot-formatted">
        <div className="loot-section-title">Reverter Defesas</div>
        <div className="loot-kv"><span className="loot-k">Exclusao Removida</span><span>{data.exc_removed}</span></div>
        <div className="loot-kv"><span className="loot-k">FW Inbound Removido</span><span>{data.fwIn_removed}</span></div>
        <div className="loot-kv"><span className="loot-k">FW Outbound Removido</span><span>{data.fwOut_removed}</span></div>
        <div className="loot-kv"><span className="loot-k">Pasta Removida</span><span>{data.dir_removed}</span></div>
      </div>
    );
  }

  // Admin enum: {current, admins, users}
  if (data.current !== undefined && data.admins !== undefined) {
    const admins = toArr(data.admins);
    const users = toArr(data.users);
    return (
      <div className="loot-formatted">
        <div className="loot-kv"><span className="loot-k">Usuario Atual</span><span>{data.current}</span></div>
        <div className="loot-section-title">Administradores</div>
        <pre className="loot-data">{admins.length > 0 ? admins.join("\n") : "Nenhum encontrado"}</pre>
        {users.length > 0 && (
          <>
            <div className="loot-section-title">Usuarios Locais</div>
            <table className="loot-table">
              <thead><tr><th>Nome</th><th>Ativo</th><th>Ultimo Logon</th></tr></thead>
              <tbody>
                {users.map((u: any, i: number) => (
                  <tr key={i}><td>{u.n}</td><td>{u.e ? "Sim" : "Nao"}</td><td>{u.l || "-"}</td></tr>
                ))}
              </tbody>
            </table>
          </>
        )}
      </div>
    );
  }

  // Create admin: {user_created, admin_added, username}
  if (data.user_created !== undefined && data.username !== undefined) {
    return (
      <div className="loot-formatted">
        <div className="loot-section-title">Criar Usuario Admin</div>
        <div className="loot-kv"><span className="loot-k">Usuario</span><span>{data.username}</span></div>
        <div className="loot-kv"><span className="loot-k">Criado</span><span>{data.user_created}</span></div>
        <div className="loot-kv"><span className="loot-k">Admin</span><span>{data.admin_added}</span></div>
      </div>
    );
  }

  // COM ports: {ports, devices}
  if (data.ports !== undefined && data.devices !== undefined) {
    const devs = toArr(data.devices);
    const ports = toArr(data.ports);
    return (
      <div className="loot-formatted">
        <div className="loot-section-title">Dispositivos Serial</div>
        {devs.length > 0 ? (
          <table className="loot-table">
            <thead><tr><th>Dispositivo</th></tr></thead>
            <tbody>
              {devs.map((d: string, i: number) => (
                <tr key={i}><td>{d}</td></tr>
              ))}
            </tbody>
          </table>
        ) : (
          <pre className="loot-data">Nenhum dispositivo encontrado</pre>
        )}
        {ports.length > 0 && (
          <div className="loot-kv">
            <span className="loot-k">Portas</span><span>{ports.join(", ")}</span>
          </div>
        )}
      </div>
    );
  }

  // Basic recon: {user, host, ip}
  if (data.user !== undefined && data.host !== undefined) {
    return (
      <div className="loot-formatted">
        <div className="loot-kv"><span className="loot-k">Usuario</span><span>{data.user}</span></div>
        <div className="loot-kv"><span className="loot-k">Hostname</span><span>{data.host}</span></div>
        {data.ip && (
          <>
            <div className="loot-section-title">Configuracao IP</div>
            <pre className="loot-data">{data.ip.trim()}</pre>
          </>
        )}
      </div>
    );
  }

  // Network recon: {arp, netstat, routes}
  if (data.arp !== undefined || data.netstat !== undefined) {
    return (
      <div className="loot-formatted">
        {data.arp && (
          <>
            <div className="loot-section-title">Tabela ARP</div>
            <pre className="loot-data">{data.arp.trim()}</pre>
          </>
        )}
        {data.netstat && (
          <>
            <div className="loot-section-title">Netstat</div>
            <pre className="loot-data">{data.netstat.trim()}</pre>
          </>
        )}
        {data.routes && (
          <>
            <div className="loot-section-title">Rotas</div>
            <pre className="loot-data">{data.routes.trim()}</pre>
          </>
        )}
      </div>
    );
  }

  // Generic JSON
  return (
    <pre className="loot-data loot-formatted">
      {JSON.stringify(data, null, 2)}
    </pre>
  );
}

function buildReportHtml(data: any): string {
  // WiFi
  if (Array.isArray(data) && data.length > 0 && data[0]?.s !== undefined) {
    let rows = data.map((w: any) => `<tr><td>${esc(w.s)}</td><td>${esc(w.k || "-")}</td></tr>`).join("");
    return `<table><thead><tr><th>SSID</th><th>Senha</th></tr></thead><tbody>${rows}</tbody></table>`;
  }
  // Port scan
  if (data.gw !== undefined && data.listen) {
    let h = `<div class="kv"><span class="k">Gateway</span><span>${esc(data.gw || "N/A")}</span></div>`;
    if (data.scan?.length > 0)
      h += `<div class="kv"><span class="k">Abertas no GW</span><span>${esc(data.scan.join(", "))}</span></div>`;
    const fwArr = toArr(data.fw);
    if (fwArr.length > 0) {
      h += `<div class="st">Regras Allow em Portas Criticas</div><table><thead><tr><th>Regra</th><th>Direcao</th><th>Porta</th></tr></thead><tbody>`;
      h += fwArr.map((f: any) => `<tr><td>${esc(f.n)}</td><td>${esc(f.d)}</td><td>${esc(f.p || "-")}</td></tr>`).join("");
      h += `</tbody></table>`;
    }
    if (data.listen?.length > 0) {
      h += `<div class="st">Portas em Escuta</div><table><thead><tr><th>Porta</th><th>Processo</th></tr></thead><tbody>`;
      h += data.listen.map((l: any) => `<tr><td>${l.p ?? l.port ?? "?"}</td><td>${esc(l.n ?? l.proc ?? "-")}</td></tr>`).join("");
      h += `</tbody></table>`;
    }
    return h;
  }
  // Defenders
  if (data.edr !== undefined || data.svc !== undefined) {
    let h = `<div class="st">Processos EDR/AV</div><pre>${Array.isArray(data.edr) ? (data.edr.length > 0 ? esc(data.edr.join(", ")) : "Nenhum detectado") : esc(String(data.edr ?? "Nenhum"))}</pre>`;
    if (data.svc && Array.isArray(data.svc)) {
      h += `<div class="st">Servicos de Seguranca</div><table><thead><tr><th>Servico</th><th>Status</th></tr></thead><tbody>`;
      h += data.svc.map((s: any) => `<tr><td>${esc(s.Name)}</td><td>${s.Status === 4 ? "Running" : s.Status === 1 ? "Stopped" : s.Status}</td></tr>`).join("");
      h += `</tbody></table>`;
    }
    return h;
  }
  // Weaken defenses
  if (data.exc !== undefined && data.fwIn !== undefined) {
    let h = `<div class="st">Diminuir Defesas</div>`;
    h += `<div class="kv"><span class="k">Exclusao Defender</span><span>${esc(data.exc)}</span></div>`;
    h += `<div class="kv"><span class="k">FW Inbound 4444</span><span>${esc(data.fwIn)}</span></div>`;
    h += `<div class="kv"><span class="k">FW Outbound 4444</span><span>${esc(data.fwOut)}</span></div>`;
    if (data.excList) h += `<div class="st">Exclusoes Ativas</div><pre>${esc(toArr(data.excList).join("\n"))}</pre>`;
    return h;
  }
  // Revert defenses
  if (data.exc_removed !== undefined && data.fwIn_removed !== undefined) {
    let h = `<div class="st">Reverter Defesas</div>`;
    h += `<div class="kv"><span class="k">Exclusao Removida</span><span>${esc(data.exc_removed)}</span></div>`;
    h += `<div class="kv"><span class="k">FW Inbound Removido</span><span>${esc(data.fwIn_removed)}</span></div>`;
    h += `<div class="kv"><span class="k">FW Outbound Removido</span><span>${esc(data.fwOut_removed)}</span></div>`;
    h += `<div class="kv"><span class="k">Pasta Removida</span><span>${esc(data.dir_removed)}</span></div>`;
    return h;
  }
  // Admin enum
  if (data.current !== undefined && data.admins !== undefined) {
    const admins = toArr(data.admins);
    const users = toArr(data.users);
    let h = `<div class="kv"><span class="k">Usuario Atual</span><span>${esc(data.current)}</span></div>`;
    h += `<div class="st">Administradores</div><pre>${admins.length > 0 ? esc(admins.join("\n")) : "Nenhum encontrado"}</pre>`;
    if (users.length > 0) {
      h += `<div class="st">Usuarios Locais</div><table><thead><tr><th>Nome</th><th>Ativo</th><th>Ultimo Logon</th></tr></thead><tbody>`;
      h += users.map((u: any) => `<tr><td>${esc(u.n)}</td><td>${u.e ? "Sim" : "Nao"}</td><td>${esc(u.l || "-")}</td></tr>`).join("");
      h += `</tbody></table>`;
    }
    return h;
  }
  // Create admin
  if (data.user_created !== undefined && data.username !== undefined) {
    let h = `<div class="st">Criar Usuario Admin</div>`;
    h += `<div class="kv"><span class="k">Usuario</span><span>${esc(data.username)}</span></div>`;
    h += `<div class="kv"><span class="k">Criado</span><span>${esc(data.user_created)}</span></div>`;
    h += `<div class="kv"><span class="k">Admin</span><span>${esc(data.admin_added)}</span></div>`;
    return h;
  }
  // COM ports
  if (data.ports !== undefined && data.devices !== undefined) {
    const devs = toArr(data.devices);
    const ports = toArr(data.ports);
    let h = `<div class="st">Dispositivos Serial</div>`;
    if (devs.length > 0) {
      h += `<table><thead><tr><th>Dispositivo</th></tr></thead><tbody>`;
      h += devs.map((d: string) => `<tr><td>${esc(d)}</td></tr>`).join("");
      h += `</tbody></table>`;
    }
    if (ports.length > 0)
      h += `<div class="kv"><span class="k">Portas</span><span>${esc(ports.join(", "))}</span></div>`;
    return h;
  }
  // Basic recon
  if (data.user !== undefined && data.host !== undefined) {
    let h = `<div class="kv"><span class="k">Usuario</span><span>${esc(data.user)}</span></div>`;
    h += `<div class="kv"><span class="k">Hostname</span><span>${esc(data.host)}</span></div>`;
    if (data.ip) h += `<div class="st">Configuracao IP</div><pre>${esc(data.ip.trim())}</pre>`;
    return h;
  }
  // Network recon
  if (data.arp !== undefined || data.netstat !== undefined) {
    let h = "";
    if (data.arp) h += `<div class="st">Tabela ARP</div><pre>${esc(data.arp.trim())}</pre>`;
    if (data.netstat) h += `<div class="st">Netstat</div><pre>${esc(data.netstat.trim())}</pre>`;
    if (data.routes) h += `<div class="st">Rotas</div><pre>${esc(data.routes.trim())}</pre>`;
    return h;
  }
  return `<pre>${esc(JSON.stringify(data, null, 2))}</pre>`;
}

export function LootDashboard() {
  const [loot, setLoot] = useState<LootEntry[]>([]);
  const [loading, setLoading] = useState(true);
  const [expanded, setExpanded] = useState<number | null>(null);

  const load = async () => {
    try {
      const data = await fetchLoot();
      setLoot(data);
    } catch {
      /* silent */
    } finally {
      setLoading(false);
    }
  };

  useEffect(() => {
    let active = true;
    const poll = async () => {
      try {
        const data = await fetchLoot();
        if (active) setLoot(data);
      } catch {
        /* silent */
      } finally {
        if (active) setLoading(false);
      }
    };
    poll();
    const id = setInterval(poll, 5000);
    return () => {
      active = false;
      clearInterval(id);
    };
  }, []);

  const exportPdf = () => {
    if (loot.length === 0) return;
    const sections = loot.map((l) => {
      let content: string;
      try {
        const parsed = JSON.parse(l.data);
        content = buildReportHtml(parsed);
      } catch {
        content = `<pre>${esc(l.data)}</pre>`;
      }
      return `<div class="entry"><h2>#${l.id} <span class="badge">${esc(l.type)}</span> <span class="time">${esc(l.timestamp)}</span></h2>${content}</div>`;
    }).join("");

    const html = `<!DOCTYPE html><html><head><meta charset="utf-8"><title>S3-Injector Loot Report</title><style>
body{font-family:'Courier New',monospace;background:#fff;color:#111;padding:2rem;max-width:800px;margin:0 auto}
h1{font-size:1.2rem;border-bottom:2px solid #000;padding-bottom:.5rem}
h2{font-size:.9rem;margin-top:1.5rem;border-bottom:1px solid #ddd;padding-bottom:.3rem}
.badge{background:#eee;padding:.1rem .4rem;border-radius:3px;font-size:.75rem}
.time{color:#888;font-size:.75rem}
table{width:100%;border-collapse:collapse;margin:.5rem 0}
th,td{text-align:left;padding:.3rem .5rem;border-bottom:1px solid #ddd;font-size:.8rem}
th{font-weight:700;text-transform:uppercase;font-size:.7rem;background:#f5f5f5}
pre{background:#f5f5f5;padding:.5rem;border-radius:3px;font-size:.75rem;white-space:pre-wrap;word-break:break-all}
.kv{display:flex;gap:1rem;font-size:.8rem;padding:.15rem 0}
.kv .k{color:#888;min-width:100px}
.st{color:#888;font-size:.7rem;text-transform:uppercase;letter-spacing:.05em;margin-top:.75rem;margin-bottom:.25rem}
.entry{page-break-inside:avoid;margin-bottom:1rem}
@media print{body{padding:0}}
</style></head><body>
<h1>S3-INJECTOR &mdash; Relatorio de Loot</h1>
<p style="color:#888;font-size:.8rem">Gerado em: ${new Date().toLocaleString("pt-BR")}</p>
${sections}
</body></html>`;

    const w = window.open("", "_blank");
    if (!w) return;
    w.document.write(html);
    w.document.close();
    setTimeout(() => w.print(), 400);
  };

  const handleClear = async () => {
    await clearLoot();
    setLoot([]);
  };

  const handleTest = async () => {
    await testLoot();
    load();
  };

  return (
    <section className="panel">
      <div className="panel-header">
        <h2 className="panel-title">Loot Dashboard</h2>
        <div className="panel-actions">
          <button className="btn-export" onClick={handleTest}>Teste</button>
          <button className="btn-danger" onClick={handleClear} disabled={loot.length === 0}>Limpar</button>
          <button className="btn-export" onClick={exportPdf} disabled={loot.length === 0}>Exportar PDF</button>
        </div>
      </div>

      {loading ? (
        <p className="muted">Carregando...</p>
      ) : loot.length === 0 ? (
        <p className="muted">Nenhum loot capturado.</p>
      ) : (
        <div className="loot-list">
          {loot.map((l) => {
            const formatted = renderLoot(l.data);
            return (
              <div
                key={l.id}
                className={`loot-card ${expanded === l.id ? "expanded" : ""}`}
                onClick={() => setExpanded(expanded === l.id ? null : l.id)}
              >
                <div className="loot-header">
                  <span className="loot-id">#{l.id}</span>
                  <span className="type-badge">{l.type}</span>
                  <span className="loot-time">{l.timestamp}</span>
                  <span className="loot-size">{l.data.length}B</span>
                </div>
                {formatted || <pre className="loot-data">{l.data}</pre>}
              </div>
            );
          })}
        </div>
      )}
    </section>
  );
}
