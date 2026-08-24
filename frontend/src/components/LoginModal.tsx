import { useState } from "react";
import { login } from "../api";

interface Props {
  onLogin: () => void;
}

export function LoginModal({ onLogin }: Props) {
  const [user, setUser] = useState("");
  const [pass, setPass] = useState("");
  const [error, setError] = useState("");
  const [loading, setLoading] = useState(false);

  async function handleSubmit(e: React.FormEvent) {
    e.preventDefault();
    setError("");
    setLoading(true);
    try {
      await login(user, pass);
      onLogin();
    } catch {
      setError("Credenciais invalidas");
    } finally {
      setLoading(false);
    }
  }

  return (
    <div className="login-overlay">
      <form className="login-modal" onSubmit={handleSubmit}>
        <h1 className="login-logo">S3-INJECTOR</h1>
        <input
          className="login-input"
          type="text"
          placeholder="Usuario"
          autoComplete="username"
          value={user}
          onChange={(e) => setUser(e.target.value)}
        />
        <input
          className="login-input"
          type="password"
          placeholder="Senha"
          autoComplete="current-password"
          value={pass}
          onChange={(e) => setPass(e.target.value)}
        />
        {error && <div className="login-error">{error}</div>}
        <button className="login-btn" type="submit" disabled={loading || !user || !pass}>
          {loading ? "..." : "ENTRAR"}
        </button>
      </form>
    </div>
  );
}
