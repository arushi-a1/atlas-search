export default function Results({ data }) {
  if (data === null) return null;

  if (data.results.length === 0) {
    return <div className="empty-state">No results for "{data.query}".</div>;
  }

  return (
    <div>
      <div className="meta-row">
        {data.count} result{data.count === 1 ? "" : "s"} for "{data.query}"
      </div>
      {data.results.map((r) => (
        <div className="result" key={r.url}>
          <div className="result-url">{r.url}</div>
          <a className="result-title" href={r.url} target="_blank" rel="noreferrer">
            {r.title || r.url}
          </a>
          <p className="result-snippet">{r.snippet}</p>
          <div className="result-score">relevance: {r.score.toFixed(3)}</div>
        </div>
      ))}
    </div>
  );
}
