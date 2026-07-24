import { useState } from "react";
import SearchBar from "./components/SearchBar.jsx";
import Results from "./components/Results.jsx";

const API_BASE_URL = import.meta.env.VITE_API_BASE_URL || "http://localhost:8080";

export default function App() {
  const [data, setData] = useState(null);
  const [loading, setLoading] = useState(false);
  const [error, setError] = useState(null);

  async function runSearch(query) {
    setLoading(true);
    setError(null);
    try {
      const url = `${API_BASE_URL}/api/search?q=${encodeURIComponent(query)}&k=20`;
      const resp = await fetch(url);
      if (!resp.ok) {
        const body = await resp.json().catch(() => ({}));
        throw new Error(body.error || `Request failed with status ${resp.status}`);
      }
      const json = await resp.json();
      setData(json);
    } catch (err) {
      setError(err.message);
      setData(null);
    } finally {
      setLoading(false);
    }
  }

  return (
    <div className="page">
      <div className="brand">
        distributed<span>search</span>
      </div>
      <SearchBar onSearch={runSearch} loading={loading} />
      <div className="hint">Wrap terms in quotes for exact phrase search.</div>
      {error && <div className="error-banner">{error}</div>}
      <Results data={data} />
    </div>
  );
}
