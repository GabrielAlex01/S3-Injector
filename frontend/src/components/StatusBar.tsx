import { useEffect, useState } from "react";
import { fetchStatus, type DeviceStatus } from "../api";

export function StatusBar() {
  const [status, setStatus] = useState<DeviceStatus | null>(null);
  const [error, setError] = useState(false);

  useEffect(() => {
    let active = true;
    const poll = async () => {
      try {
        const s = await fetchStatus();
        if (active) {
          setStatus(s);
          setError(false);
        }
      } catch {
        if (active) setError(true);
      }
    };
    poll();
    const id = setInterval(poll, 3000);
    return () => {
      active = false;
      clearInterval(id);
    };
  }, []);

  if (error) return <div className="status-bar offline"><span className="dot" /> OFFLINE</div>;
  if (!status) return <div className="status-bar">...</div>;

  return (
    <div className={`status-bar ${status.cdc_connected ? "online" : "offline"}`}>
      <span className="dot" />
      <span>OTG {status.cdc_connected ? "ON" : "OFF"}</span>
    </div>
  );
}
