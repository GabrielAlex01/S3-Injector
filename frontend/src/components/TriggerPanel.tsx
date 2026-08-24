import { useEffect, useRef, useState } from "react";
import {
  executePayload,
  fetchPayloads,
  createPayload,
  deletePayload,
  type BuiltinPayload,
  type CustomPayload,
} from "../api";

export function TriggerPanel() {
  const [builtins, setBuiltins] = useState<BuiltinPayload[]>([]);
  const [customs, setCustoms] = useState<CustomPayload[]>([]);
  const [firing, setFiring] = useState<string | null>(null);
  const [result, setResult] = useState<string | null>(null);
  const [showEditor, setShowEditor] = useState(false);
  const [editTitle, setEditTitle] = useState("");
  const [editScript, setEditScript] = useState("");
  const fileRef = useRef<HTMLInputElement>(null);

  const loadPayloads = async () => {
    try {
      const data = await fetchPayloads();
      setBuiltins(data.builtin);
      setCustoms(data.custom);
    } catch {
      /* offline */
    }
  };

  useEffect(() => {
    loadPayloads();
  }, []);

  const fire = async (id: string) => {
    setFiring(id);
    setResult(null);
    try {
      const res = await executePayload(id);
      setResult(res.queued ? "Na fila — injetando keystrokes..." : "Rejeitado");
    } catch (e) {
      setResult(`Erro: ${e instanceof Error ? e.message : "desconhecido"}`);
    } finally {
      setFiring(null);
    }
  };

  const handleSave = async () => {
    if (!editTitle.trim() || !editScript.trim()) return;
    await createPayload(editTitle.trim(), editScript);
    setEditTitle("");
    setEditScript("");
    setShowEditor(false);
    loadPayloads();
  };

  const handleDelete = async (id: number) => {
    await deletePayload(id);
    loadPayloads();
  };

  const handleFile = (e: React.ChangeEvent<HTMLInputElement>) => {
    const file = e.target.files?.[0];
    if (!file) return;
    const reader = new FileReader();
    reader.onload = (ev) => {
      setEditScript(ev.target?.result as string);
      if (!editTitle) setEditTitle(file.name.replace(/\.txt$/i, ""));
    };
    reader.readAsText(file);
    e.target.value = "";
  };

  return (
    <section className="panel">
      <div className="panel-header">
        <h2 className="panel-title">Payloads</h2>
        <button
          className="btn-export"
          onClick={() => setShowEditor(!showEditor)}
        >
          {showEditor ? "Cancelar" : "+ Novo Payload"}
        </button>
      </div>

      {showEditor && (
        <div className="editor-card">
          <input
            className="editor-input"
            placeholder="Titulo do payload"
            value={editTitle}
            onChange={(e) => setEditTitle(e.target.value)}
          />
          <textarea
            className="editor-textarea"
            placeholder={"DuckyScript — ex:\nGUI x\nDELAY 500\nDOWNARROW\nREPEAT 9\nSTRING whoami\nENTER"}
            value={editScript}
            onChange={(e) => setEditScript(e.target.value)}
            rows={12}
          />
          <div className="editor-actions">
            <button
              className="btn-upload"
              onClick={() => fileRef.current?.click()}
            >
              Upload .txt
            </button>
            <input
              ref={fileRef}
              type="file"
              accept=".txt"
              style={{ display: "none" }}
              onChange={handleFile}
            />
            <button
              className="btn-save"
              onClick={handleSave}
              disabled={!editTitle.trim() || !editScript.trim()}
            >
              Salvar
            </button>
          </div>
        </div>
      )}

      <div className="payload-grid">
        {builtins.map((p) => (
          <button
            key={p.id}
            className="payload-card"
            disabled={firing !== null}
            onClick={() => fire(p.id)}
          >
            <div className="payload-header">
              <span className="payload-label">{p.label}</span>
            </div>
            <p className="payload-desc">{p.description}</p>
            {firing === p.id && <span className="firing">INJETANDO...</span>}
          </button>
        ))}

        {customs.map((cp) => (
          <div key={`c-${cp.id}`} className="payload-card custom-card">
            <div className="payload-header">
              <span className="payload-label">{cp.title}</span>
              <button
                className="delete-btn"
                onClick={(e) => {
                  e.stopPropagation();
                  handleDelete(cp.id);
                }}
              >
                X
              </button>
            </div>
            <p className="payload-desc">Payload DuckyScript personalizado</p>
            <button
              className="fire-btn"
              disabled={firing !== null}
              onClick={() => fire(`custom:${cp.id}`)}
            >
              {firing === `custom:${cp.id}` ? "INJETANDO..." : "EXECUTAR"}
            </button>
          </div>
        ))}
      </div>

      {result && <div className="result-toast">{result}</div>}
    </section>
  );
}
