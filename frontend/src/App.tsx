import { useState, useEffect } from "react";
import { StatusBar } from "./components/StatusBar";
import { LootDashboard } from "./components/LootDashboard";
import { TriggerPanel } from "./components/TriggerPanel";
import { LoginModal } from "./components/LoginModal";
import { getToken, clearToken, fetchStatus } from "./api";

type Tab = "payloads" | "loot";

export function App() {
  const [authed, setAuthed] = useState(false);
  const [checking, setChecking] = useState(true);
  const [tab, setTab] = useState<Tab>("payloads");

  useEffect(() => {
    if (!getToken()) {
      setChecking(false);
      return;
    }
    fetchStatus()
      .then(() => setAuthed(true))
      .catch(() => clearToken())
      .finally(() => setChecking(false));
  }, []);

  if (checking) return null;

  if (!authed) return <LoginModal onLogin={() => setAuthed(true)} />;

  return (
    <>
      <header className="header">
        <h1 className="logo">S3-INJECTOR</h1>
        <StatusBar />
      </header>

      <nav className="tabs">
        <button
          className={`tab ${tab === "payloads" ? "active" : ""}`}
          onClick={() => setTab("payloads")}
        >
          Payloads
        </button>
        <button
          className={`tab ${tab === "loot" ? "active" : ""}`}
          onClick={() => setTab("loot")}
        >
          Loot
        </button>
      </nav>

      <main>
        {tab === "payloads" && <TriggerPanel />}
        {tab === "loot" && <LootDashboard />}
      </main>
    </>
  );
}
