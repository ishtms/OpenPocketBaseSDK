import { useEffect, useMemo, useRef, useState } from "react";
import { docs, docsBySlug, navigation, searchDocs, type DocBlock, type DocPage } from "./docs";
import { Icon, OpenMobileMark, type IconName } from "./icons";

const sourceUrl = "https://github.com/ishtms/pb_sdk_private";

const readSlug = () => {
  const raw = window.location.hash.replace(/^#\/?/, "");
  return raw.startsWith("docs/") ? raw.slice(5) : raw || "overview";
};

const hrefFor = (slug: string) => `#/docs/${slug}`;

function useRoute() {
  const [slug, setSlug] = useState(readSlug);

  useEffect(() => {
    const onHashChange = () => setSlug(readSlug());
    window.addEventListener("hashchange", onHashChange);
    if (!window.location.hash) {
      window.history.replaceState(null, "", hrefFor("overview"));
    }
    return () => window.removeEventListener("hashchange", onHashChange);
  }, []);

  return docsBySlug.has(slug) ? slug : "overview";
}

function ProductRail({ onMenu }: { onMenu: () => void }) {
  const products: { name: string; icon: IconName; active?: boolean; soon?: boolean }[] = [
    { name: "PocketBase", icon: "database", active: true },
    { name: "OpenHaptics", icon: "haptics", soon: true },
    { name: "OpenSensors", icon: "sensor", soon: true },
    { name: "OpenAds", icon: "zap", soon: true },
  ];

  return (
    <aside className="product-rail" aria-label="OpenMobile products">
      <a className="rail-mark" href={hrefFor("overview")} aria-label="OpenMobile docs home">
        <OpenMobileMark />
      </a>
      <button className="rail-menu" onClick={onMenu} aria-label="Open documentation navigation">
        <Icon name="menu" />
      </button>
      <div className="rail-products">
        {products.map((product) => (
          <button
            key={product.name}
            className={`rail-product ${product.active ? "is-active" : ""}`}
            title={product.soon ? `${product.name}, coming soon` : product.name}
            aria-label={product.soon ? `${product.name}, coming soon` : product.name}
          >
            <Icon name={product.icon} />
            {product.soon && <span className="rail-dot" />}
          </button>
        ))}
      </div>
      <div className="rail-bottom">
        <a href={sourceUrl} target="_blank" rel="noreferrer" aria-label="Open repository">
          <Icon name="github" />
        </a>
      </div>
    </aside>
  );
}

function Sidebar({ activeSlug, open, onClose, onSearch }: { activeSlug: string; open: boolean; onClose: () => void; onSearch: () => void }) {
  return (
    <>
      <button className={`sidebar-scrim ${open ? "is-open" : ""}`} onClick={onClose} aria-label="Close navigation" />
      <aside className={`docs-sidebar ${open ? "is-open" : ""}`}>
        <div className="sidebar-brand">
          <a href={hrefFor("overview")} onClick={onClose}>
            <span className="brand-name">OPENMOBILE</span>
            <span className="brand-subtitle">DEVELOPER DOCS</span>
          </a>
          <button className="sidebar-close" onClick={onClose} aria-label="Close navigation">
            <Icon name="x" />
          </button>
        </div>

        <div className="product-select">
          <span className="product-select-icon"><Icon name="database" /></span>
          <span><strong>OpenPocketBase</strong><small>Unreal Engine SDK</small></span>
          <Icon name="chevron-down" />
        </div>

        <button className="search-trigger" onClick={onSearch}>
          <Icon name="search" />
          <span>Search documentation</span>
          <kbd><Icon name="command" /> K</kbd>
        </button>

        <nav className="sidebar-nav" aria-label="Documentation">
          {navigation.map((group) => (
            <div className="nav-group" key={group.title}>
              <h2>{group.title}</h2>
              {group.items.map((item) => (
                <a
                  key={item.slug}
                  className={activeSlug === item.slug ? "is-active" : ""}
                  href={hrefFor(item.slug)}
                  onClick={onClose}
                >
                  <span>{item.title}</span>
                  {activeSlug === item.slug && <Icon name="chevron-right" />}
                </a>
              ))}
            </div>
          ))}
        </nav>

        <div className="sidebar-meta">
          <span className="status-light" />
          <span>SDK 0.1.0</span>
          <span className="meta-divider" />
          <span>PB 0.39.11</span>
        </div>
      </aside>
    </>
  );
}

function Topbar({ page, onMenu, onSearch }: { page: DocPage; onMenu: () => void; onSearch: () => void }) {
  return (
    <header className="topbar">
      <div className="topbar-path">
        <button className="mobile-menu" onClick={onMenu} aria-label="Open navigation"><Icon name="menu" /></button>
        <span>OpenMobile</span>
        <Icon name="chevron-right" />
        <span>PocketBase</span>
        <Icon name="chevron-right" />
        <strong>{page.title}</strong>
      </div>
      <div className="topbar-actions">
        <button className="topbar-search" onClick={onSearch} aria-label="Search documentation"><Icon name="search" /><span>Search</span><kbd>⌘ K</kbd></button>
        <a href={sourceUrl} target="_blank" rel="noreferrer"><Icon name="github" /><span>Source</span><Icon name="external" /></a>
      </div>
    </header>
  );
}

function SearchDialog({ open, onClose }: { open: boolean; onClose: () => void }) {
  const [query, setQuery] = useState("");
  const inputRef = useRef<HTMLInputElement>(null);
  const results = useMemo(() => searchDocs(query), [query]);

  useEffect(() => {
    if (open) {
      setQuery("");
      window.setTimeout(() => inputRef.current?.focus(), 20);
    }
  }, [open]);

  if (!open) return null;

  return (
    <div className="search-overlay" role="dialog" aria-modal="true" aria-label="Search documentation" onMouseDown={onClose}>
      <div className="search-dialog" onMouseDown={(event) => event.stopPropagation()}>
        <div className="search-field">
          <Icon name="search" />
          <input
            ref={inputRef}
            value={query}
            onChange={(event) => setQuery(event.target.value)}
            placeholder="Search nodes, workflows, errors, and types..."
            aria-label="Search query"
          />
          <button onClick={onClose}>ESC</button>
        </div>
        <div className="search-label">{query ? `${results.length} BEST MATCHES` : "QUICK ACCESS"}</div>
        <div className="search-results">
          {results.map((result, index) => (
            <a key={result.slug} href={hrefFor(result.slug)} onClick={onClose}>
              <span className="result-number">{String(index + 1).padStart(2, "0")}</span>
              <span className="result-copy"><small>{result.eyebrow}</small><strong>{result.title}</strong><p>{result.description}</p></span>
              <Icon name="arrow-right" />
            </a>
          ))}
          {results.length === 0 && (
            <div className="search-empty"><Icon name="search" /><strong>No exact match</strong><p>Try a node name, type, or workflow such as refresh, upload, or batch.</p></div>
          )}
        </div>
        <div className="search-footer"><span>Searches all {docs.length} pages</span><span><kbd>ENTER</kbd> open <kbd>ESC</kbd> close</span></div>
      </div>
    </div>
  );
}

function CodeBlock({ block, onCopied }: { block: Extract<DocBlock, { type: "code" }>; onCopied: () => void }) {
  const [copied, setCopied] = useState(false);
  const copy = async () => {
    await navigator.clipboard.writeText(block.code);
    setCopied(true);
    onCopied();
    window.setTimeout(() => setCopied(false), 1500);
  };
  const lines = block.code.split("\n");

  return (
    <div className="code-block">
      <div className="code-topline">
        <span><Icon name="terminal" />{block.label ?? block.language.toUpperCase()}</span>
        <button onClick={copy}><Icon name={copied ? "check" : "clipboard"} />{copied ? "Copied" : "Copy"}</button>
      </div>
      <pre><code>{lines.map((line, index) => <span className="code-line" key={index}><i>{index + 1}</i><b>{line || " "}</b></span>)}</code></pre>
    </div>
  );
}

function RichBlock({ block, onCopied }: { block: DocBlock; onCopied: () => void }) {
  switch (block.type) {
    case "lead":
      return <p className="section-lead">{block.text}</p>;
    case "paragraph":
      return <p>{block.text}</p>;
    case "bullets":
      return <ul className="doc-bullets">{block.items.map((item) => <li key={item}><span><Icon name="chevron-right" /></span>{item}</li>)}</ul>;
    case "steps":
      return <ol className="doc-steps">{block.items.map((item, index) => <li key={item.title}><span>{String(index + 1).padStart(2, "0")}</span><div><strong>{item.title}</strong><p>{item.text}</p></div></li>)}</ol>;
    case "code":
      return <CodeBlock block={block} onCopied={onCopied} />;
    case "callout":
      return <aside className={`callout callout-${block.tone}`}><span className="callout-icon"><Icon name={block.tone === "success" ? "check" : block.tone === "danger" ? "shield" : block.tone === "warning" ? "zap" : "info"} /></span><div><strong>{block.title}</strong><p>{block.text}</p></div></aside>;
    case "screenshot":
      return <div className="screenshot-placeholder"><div className="placeholder-grid" /><span className="placeholder-icon"><Icon name="grid" /></span><strong>SCREENSHOT PLACEHOLDER</strong><p>{block.text}</p><small>REPLACE WITH FINAL BLUEPRINT CAPTURE</small></div>;
    case "table":
      return <div className="table-wrap"><table><thead><tr>{block.columns.map((column) => <th key={column}>{column}</th>)}</tr></thead><tbody>{block.rows.map((row, index) => <tr key={index}>{row.map((cell, cellIndex) => <td key={cellIndex}>{cell}</td>)}</tr>)}</tbody></table></div>;
    case "cards":
      return <div className="doc-cards">{block.items.map((item) => item.link ? <a href={hrefFor(item.link)} key={item.title}><span className="card-top"><small>{item.label ?? "GUIDE"}</small><Icon name="arrow-right" /></span><strong>{item.title}</strong><p>{item.text}</p></a> : <div key={item.title}><strong>{item.title}</strong><p>{item.text}</p></div>)}</div>;
  }
}

function PageHeader({ page }: { page: DocPage }) {
  const isOverview = page.slug === "overview";
  return (
    <div className={`page-header ${isOverview ? "overview-header" : ""}`}>
      <div className="header-coordinate"><span>DOC / {page.slug.toUpperCase()}</span><span>V0.1.0</span></div>
      <div className="eyebrow"><span /><strong>{page.eyebrow}</strong></div>
      <h1>{page.title}</h1>
      <p>{page.description}</p>
      <div className="page-badges">
        <span><Icon name="book" />{page.readTime} read</span>
        <span><Icon name="check" />PocketBase 0.39.11</span>
        <span><Icon name="code" />C++ + Blueprint</span>
      </div>
      {isOverview && (
        <div className="overview-console">
          <div className="console-line"><span>OPENMOBILE::POCKETBASE</span><span className="console-status">READY</span></div>
          <div className="console-stats">
            <div><strong>31</strong><span>GUIDES</span></div>
            <div><strong>02</strong><span>LANGUAGES</span></div>
            <div><strong>01</strong><span>CLIENT</span></div>
            <div><strong>100%</strong><span>OPEN SOURCE</span></div>
          </div>
        </div>
      )}
    </div>
  );
}

function OnThisPage({ page, activeSection }: { page: DocPage; activeSection: string }) {
  const scrollToSection = (sectionId: string) => {
    document.getElementById(`section-${sectionId}`)?.scrollIntoView({ behavior: "smooth", block: "start" });
  };

  return (
    <aside className="on-this-page">
      <div className="toc-label">ON THIS PAGE</div>
      <nav>
        {page.sections.map((section, index) => (
          <button className={activeSection === section.id ? "is-active" : ""} onClick={() => scrollToSection(section.id)} key={section.id}>
            <span>{String(index + 1).padStart(2, "0")}</span>{section.title}
          </button>
        ))}
      </nav>
      <div className="toc-card">
        <Icon name="spark" />
        <strong>Found a gap?</strong>
        <p>Keep this page beside the SDK version it documents.</p>
        <a href={sourceUrl} target="_blank" rel="noreferrer">Open source <Icon name="external" /></a>
      </div>
    </aside>
  );
}

function PageFooter({ page }: { page: DocPage }) {
  const index = docs.findIndex((item) => item.slug === page.slug);
  const previous = docs[index - 1];
  const next = docs[index + 1];
  return (
    <footer className="page-footer">
      <div className="page-pager">
        {previous ? <a href={hrefFor(previous.slug)} className="pager-previous"><Icon name="arrow-left" /><span><small>PREVIOUS</small><strong>{previous.title}</strong></span></a> : <span />}
        {next ? <a href={hrefFor(next.slug)} className="pager-next"><span><small>NEXT</small><strong>{next.title}</strong></span><Icon name="arrow-right" /></a> : <span />}
      </div>
      <div className="footer-line"><span>OPENMOBILE DOCUMENTATION</span><span>OPENPOCKETBASE SDK 0.1.0</span></div>
    </footer>
  );
}

function App() {
  const slug = useRoute();
  const page = docsBySlug.get(slug)!;
  const [sidebarOpen, setSidebarOpen] = useState(false);
  const [searchOpen, setSearchOpen] = useState(false);
  const [activeSection, setActiveSection] = useState(page.sections[0]?.id ?? "");
  const [scrollProgress, setScrollProgress] = useState(0);
  const [toast, setToast] = useState(false);

  useEffect(() => {
    setSidebarOpen(false);
    setActiveSection(page.sections[0]?.id ?? "");
    window.scrollTo({ top: 0 });
    document.title = `${page.title} | OpenMobile Docs`;
  }, [page]);

  useEffect(() => {
    const onKeyDown = (event: KeyboardEvent) => {
      if ((event.metaKey || event.ctrlKey) && event.key.toLowerCase() === "k") {
        event.preventDefault();
        setSearchOpen(true);
      }
      if (event.key === "Escape") {
        setSearchOpen(false);
        setSidebarOpen(false);
      }
    };
    window.addEventListener("keydown", onKeyDown);
    return () => window.removeEventListener("keydown", onKeyDown);
  }, []);

  useEffect(() => {
    const onScroll = () => {
      const root = document.documentElement;
      const max = root.scrollHeight - root.clientHeight;
      setScrollProgress(max > 0 ? Math.min(1, root.scrollTop / max) : 0);

      let current = page.sections[0]?.id ?? "";
      for (const section of page.sections) {
        const element = document.getElementById(`section-${section.id}`);
        if (element && element.getBoundingClientRect().top < 190) current = section.id;
      }
      setActiveSection(current);
    };
    onScroll();
    window.addEventListener("scroll", onScroll, { passive: true });
    return () => window.removeEventListener("scroll", onScroll);
  }, [page]);

  const showCopied = () => {
    setToast(true);
    window.setTimeout(() => setToast(false), 1600);
  };

  return (
    <div className="app-shell">
      <div className="ambient-glow" />
      <div className="scroll-meter" style={{ transform: `scaleX(${scrollProgress})` }} />
      <ProductRail onMenu={() => setSidebarOpen(true)} />
      <Sidebar activeSlug={slug} open={sidebarOpen} onClose={() => setSidebarOpen(false)} onSearch={() => setSearchOpen(true)} />
      <div className="main-shell">
        <Topbar page={page} onMenu={() => setSidebarOpen(true)} onSearch={() => setSearchOpen(true)} />
        <div className="content-shell">
          <main className="doc-main">
            <PageHeader page={page} />
            <div className="doc-rule"><span>OPENPOCKETBASE / {page.slug.toUpperCase()}</span><i /></div>
            {page.sections.map((section, sectionIndex) => (
              <section className="doc-section" id={`section-${section.id}`} key={section.id}>
                <div className="section-heading"><span>{String(sectionIndex + 1).padStart(2, "0")}</span><h2>{section.title}</h2></div>
                <div className="section-body">
                  {section.blocks.map((block, blockIndex) => <RichBlock key={blockIndex} block={block} onCopied={showCopied} />)}
                </div>
              </section>
            ))}
            <PageFooter page={page} />
          </main>
          <OnThisPage page={page} activeSection={activeSection} />
        </div>
      </div>
      <SearchDialog open={searchOpen} onClose={() => setSearchOpen(false)} />
      <div className={`copy-toast ${toast ? "is-visible" : ""}`}><Icon name="check" />Copied to clipboard</div>
    </div>
  );
}

export default App;
