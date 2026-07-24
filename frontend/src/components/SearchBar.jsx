import { useState } from "react";

export default function SearchBar({ onSearch, loading }) {
  const [value, setValue] = useState("");

  function handleSubmit(e) {
    e.preventDefault();
    if (value.trim().length === 0) return;
    onSearch(value.trim());
  }

  return (
    <form className="search-form" onSubmit={handleSubmit}>
      <input
        className="search-input"
        type="text"
        placeholder='Search the index... try "an exact phrase"'
        value={value}
        onChange={(e) => setValue(e.target.value)}
        autoFocus
      />
      <button className="search-button" type="submit" disabled={loading}>
        {loading ? "Searching…" : "Search"}
      </button>
    </form>
  );
}
