import { useEffect, useMemo, useRef, useState } from "react";
import { docs, docsBySlug, navigation, searchDocs, type DocBlock, type DocPage } from "./docs";
import { Icon, PocketBaseMark } from "./icons";
import { applyTheme, getInitialTheme, nextTheme, saveTheme, type Theme } from "./theme";

const sourceUrl = "https://github.com/ishtms/OpenPocketBaseSDK";
const tutorialPages = docs.filter((page) => page.tutorial).sort((a, b) => a.tutorial!.order - b.tutorial!.order);

const primaryNavigation = [
  { title: "Docs", slug: "overview", active: (slug: string) => slug === "overview" || slug.startsWith("start/") || slug.startsWith("core/") },
  { title: "Tutorials", slug: "tutorials/getting-started", active: (slug: string) => slug.startsWith("tutorials/") },
  { title: "Guides", slug: "records/crud", active: (slug: string) => ["records/", "authentication/", "files/", "realtime/", "tools/"].some((prefix) => slug.startsWith(prefix)) },
  { title: "Blueprint API", slug: "reference/blueprint-nodes", active: (slug: string) => slug === "reference/blueprint-nodes" || slug === "reference/data-types" },
  { title: "C++ API", slug: "reference/api-index", active: (slug: string) => slug === "reference/api-index" || slug === "reference/feature-status" },
];

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
    if (!window.location.hash) window.history.replaceState(null, "", hrefFor("overview"));
    return () => window.removeEventListener("hashchange", onHashChange);
  }, []);

  return docsBySlug.has(slug) ? slug : "overview";
}

function SiteHeader({ activeSlug, theme, onMenu, onSearch, onThemeToggle }: {
  activeSlug: string;
  theme: Theme;
  onMenu: () => void;
  onSearch: () => void;
  onThemeToggle: () => void;
}) {
  return (
    <header className="site-header">
      <div className="header-inner">
        <button className="mobile-menu" onClick={onMenu} aria-label="Open documentation navigation">
          <Icon name="menu" />
        </button>
        <a className="site-brand" href={hrefFor("overview")} aria-label="OpenPocketBase documentation home">
          <span className="brand-mark"><PocketBaseMark /></span>
          <span className="brand-copy"><strong>OpenPocketBase</strong><small>Unreal Engine SDK</small></span>
        </a>

        <nav className="primary-nav" aria-label="Primary navigation">
          {primaryNavigation.map((item) => (
            <a className={item.active(activeSlug) ? "is-active" : ""} href={hrefFor(item.slug)} key={item.title}>
              {item.title}
            </a>
          ))}
        </nav>

        <div className="header-actions">
          <button className="header-search" onClick={onSearch} aria-label="Search documentation">
            <Icon name="search" /><span>Search</span><kbd>Ctrl K</kbd>
          </button>
          <button className="icon-button" onClick={onThemeToggle} aria-label={`Switch to ${theme === "dark" ? "light" : "dark"} theme`}>
            <Icon name={theme === "dark" ? "sun" : "moon"} />
          </button>
          <a className="icon-button" href={sourceUrl} target="_blank" rel="noreferrer" aria-label="Open GitHub repository">
            <Icon name="github" />
          </a>
        </div>
      </div>
    </header>
  );
}

function Sidebar({ activeSlug, open, onClose, onSearch }: {
  activeSlug: string;
  open: boolean;
  onClose: () => void;
  onSearch: () => void;
}) {
  return (
    <>
      <button className={`sidebar-scrim ${open ? "is-open" : ""}`} onClick={onClose} aria-label="Close navigation" />
      <aside className={`docs-sidebar ${open ? "is-open" : ""}`}>
        <div className="sidebar-mobile-head">
          <span><PocketBaseMark /> Documentation</span>
          <button onClick={onClose} aria-label="Close navigation"><Icon name="x" /></button>
        </div>

        <button className="sidebar-search" onClick={onSearch}>
          <Icon name="search" /><span>Search everything</span><kbd>Ctrl K</kbd>
        </button>

        <div className="sidebar-course-card">
          <span className="course-kicker"><Icon name="spark" /> Blueprint course</span>
          <strong>From zero to realtime</strong>
          <p>Nine practical lessons. One working PocketBase integration.</p>
          <a href={hrefFor("tutorials/getting-started")} onClick={onClose}>Start learning <Icon name="arrow-right" /></a>
        </div>

        <nav className="sidebar-nav" aria-label="Documentation pages">
          {navigation.map((group) => (
            <div className={`nav-group ${group.title === "Blueprint tutorials" ? "tutorial-group" : ""}`} key={group.title}>
              <h2>{group.title}</h2>
              {group.items.map((item) => (
                <a
                  className={activeSlug === item.slug ? "is-active" : ""}
                  href={hrefFor(item.slug)}
                  onClick={onClose}
                  key={item.slug}
                >
                  <span>{item.title}</span>
                  {activeSlug === item.slug && <Icon name="chevron-right" />}
                </a>
              ))}
            </div>
          ))}
        </nav>

        <div className="sidebar-footer">
          <span><i /> SDK 0.1.0</span>
          <span>PocketBase 0.39.11</span>
        </div>
      </aside>
    </>
  );
}

function SearchDialog({ open, onClose }: { open: boolean; onClose: () => void }) {
  const [query, setQuery] = useState("");
  const inputRef = useRef<HTMLInputElement>(null);
  const results = useMemo(() => searchDocs(query), [query]);

  useEffect(() => {
    if (!open) return;
    setQuery("");
    window.setTimeout(() => inputRef.current?.focus(), 20);
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
            placeholder="Search tutorials, Blueprint nodes, errors, and types..."
            aria-label="Search query"
          />
          <button onClick={onClose}>ESC</button>
        </div>
        <div className="search-summary">
          <span>{query ? `${results.length} results` : "Popular starting points"}</span>
          <small>Searching {docs.length} documentation pages</small>
        </div>
        <div className="search-results">
          {results.map((result) => (
            <a href={hrefFor(result.slug)} onClick={onClose} key={result.slug}>
              <span className="result-icon"><Icon name={result.slug.startsWith("tutorials/") ? "spark" : "book"} /></span>
              <span className="result-copy"><small>{result.eyebrow}</small><strong>{result.title}</strong><p>{result.description}</p></span>
              <Icon name="arrow-right" />
            </a>
          ))}
          {results.length === 0 && (
            <div className="search-empty"><Icon name="search" /><strong>No match yet</strong><p>Try a workflow such as login, create record, protected file, or realtime.</p></div>
          )}
        </div>
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

  return (
    <div className="code-block">
      <div className="code-topline">
        <span><Icon name="terminal" />{block.label ?? block.language.toUpperCase()}</span>
        <button onClick={copy}><Icon name={copied ? "check" : "clipboard"} />{copied ? "Copied" : "Copy"}</button>
      </div>
      <pre><code>{block.code.split("\n").map((line, index) => (
        <span className="code-line" key={`${index}-${line}`}><i>{String(index + 1).padStart(2, "0")}</i><b>{line || " "}</b></span>
      ))}</code></pre>
    </div>
  );
}

function ScreenshotBlock({ block }: { block: Extract<DocBlock, { type: "screenshot" }> }) {
  return (
    <figure className="blueprint-shot">
      <div className="shot-toolbar">
        <span><Icon name="grid" /> BLUEPRINT CAPTURE</span>
        <code>{block.asset ?? "/tutorials/replace-this-capture.png"}</code>
      </div>
      <div className="shot-canvas">
        <div className="shot-wire wire-a" />
        <div className="shot-wire wire-b" />
        <div className="fake-node node-event"><span /><b>EVENT</b><i /><i /></div>
        <div className="fake-node node-action"><span /><b>OPEN POCKETBASE</b><i /><i /><i /></div>
        <div className="shot-drop">
          <span><Icon name="grid" /></span>
          <strong>DROP YOUR BLUEPRINT SCREENSHOT HERE</strong>
          <p>{block.text}</p>
        </div>
      </div>
      <figcaption>
        <div><span>CAPTURE NOTE</span><strong>{block.title ?? "Blueprint graph"}</strong></div>
        <p>{block.caption ?? "Replace this placeholder with a tightly cropped, readable Blueprint graph capture."}</p>
      </figcaption>
    </figure>
  );
}

function RichBlock({ block, onCopied }: { block: DocBlock; onCopied: () => void }) {
  switch (block.type) {
    case "lead":
      return <p className="section-lead">{block.text}</p>;
    case "paragraph":
      return <p>{block.text}</p>;
    case "bullets":
      return <ul className="doc-bullets">{block.items.map((item) => <li key={item}><span><Icon name="check" /></span>{item}</li>)}</ul>;
    case "steps":
      return <ol className="doc-steps">{block.items.map((item, index) => <li key={item.title}><span>{String(index + 1).padStart(2, "0")}</span><div><strong>{item.title}</strong><p>{item.text}</p></div></li>)}</ol>;
    case "code":
      return <CodeBlock block={block} onCopied={onCopied} />;
    case "callout":
      return <aside className={`callout callout-${block.tone}`}><span className="callout-icon"><Icon name={block.tone === "success" ? "check" : block.tone === "danger" ? "shield" : block.tone === "warning" ? "zap" : "info"} /></span><div><strong>{block.title}</strong><p>{block.text}</p></div></aside>;
    case "screenshot":
      return <ScreenshotBlock block={block} />;
    case "table":
      return <div className="table-wrap"><table><thead><tr>{block.columns.map((column) => <th key={column}>{column}</th>)}</tr></thead><tbody>{block.rows.map((row, index) => <tr key={index}>{row.map((cell, cellIndex) => <td key={`${cellIndex}-${cell}`}>{cell}</td>)}</tr>)}</tbody></table></div>;
    case "cards":
      return <div className="doc-cards">{block.items.map((item) => item.link ? (
        <a href={hrefFor(item.link)} key={item.title}><span className="card-label">{item.label ?? "GUIDE"}</span><strong>{item.title}</strong><p>{item.text}</p><span className="card-link">Open page <Icon name="arrow-right" /></span></a>
      ) : <div key={item.title}><strong>{item.title}</strong><p>{item.text}</p></div>)}</div>;
  }
}

function TutorialHeader({ page }: { page: DocPage }) {
  if (!page.tutorial) return null;
  const total = tutorialPages.length;
  const progress = Math.round((page.tutorial.order / total) * 100);

  return (
    <div className="tutorial-meta">
      <div className="tutorial-progress">
        <span><Icon name="spark" /> Blueprint course</span>
        <strong>{String(page.tutorial.order).padStart(2, "0")} <i>/</i> {String(total).padStart(2, "0")}</strong>
        <div><i style={{ width: `${progress}%` }} /></div>
      </div>
      <div className="tutorial-outcome"><small>YOU WILL FINISH WITH</small><p>{page.tutorial.outcome}</p></div>
      <div className="tutorial-prereqs"><small>BEFORE YOU START</small><ul>{page.tutorial.prerequisites.map((item) => <li key={item}><Icon name="check" />{item}</li>)}</ul></div>
    </div>
  );
}

function PageHeader({ page }: { page: DocPage }) {
  const isOverview = page.slug === "overview";
  const category = page.slug.startsWith("tutorials/") ? "Tutorials" : page.slug.includes("/") ? page.slug.split("/")[0] : "Documentation";

  return (
    <header className={`page-header ${isOverview ? "overview-header" : ""}`}>
      <div className="breadcrumb"><a href={hrefFor("overview")}>Docs</a><Icon name="chevron-right" /><span>{category}</span></div>
      <div className="page-eyebrow"><span>{page.eyebrow}</span>{page.tutorial && <i>{page.tutorial.level}</i>}</div>
      <h1>{page.title}</h1>
      <p>{page.description}</p>
      <div className="page-badges">
        <span><Icon name="book" />{page.readTime} read</span>
        <span><Icon name="database" />PocketBase 0.39.11</span>
        <span><Icon name="grid" />Blueprint first</span>
      </div>
      {isOverview && (
        <div className="hero-actions">
          <a className="primary-action" href={hrefFor("tutorials/getting-started")}>Start the Blueprint course <Icon name="arrow-right" /></a>
          <a className="secondary-action" href={hrefFor("start/installation")}>Install the SDK</a>
        </div>
      )}
      {isOverview && (
        <div className="hero-console">
          <div className="console-head"><span><i /> OPENPOCKETBASE / READY</span><code>UE 5.8 · PB 0.39.11</code></div>
          <div className="console-flow">
            <span><small>01</small>Initialize client</span><i /><span><small>02</small>Use collection</span><i /><span><small>03</small>Call Blueprint node</span>
          </div>
        </div>
      )}
      <TutorialHeader page={page} />
    </header>
  );
}

function OnThisPage({ page, activeSection }: { page: DocPage; activeSection: string }) {
  const scrollToSection = (sectionId: string) => document.getElementById(`section-${sectionId}`)?.scrollIntoView({ behavior: "smooth", block: "start" });
  return (
    <aside className="on-this-page">
      <span className="toc-label">ON THIS PAGE</span>
      <nav>{page.sections.map((section, index) => (
        <button className={activeSection === section.id ? "is-active" : ""} onClick={() => scrollToSection(section.id)} key={section.id}>
          <span>{String(index + 1).padStart(2, "0")}</span>{section.title}
        </button>
      ))}</nav>
      {page.tutorial ? (
        <div className="toc-help"><Icon name="grid" /><strong>Adding screenshots?</strong><p>Every placeholder includes the suggested public asset path and an exact capture note.</p></div>
      ) : (
        <div className="toc-help"><Icon name="github" /><strong>Found a gap?</strong><p>Open an issue with the SDK and page version.</p><a href={sourceUrl} target="_blank" rel="noreferrer">GitHub <Icon name="external" /></a></div>
      )}
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
        {previous ? <a href={hrefFor(previous.slug)}><Icon name="arrow-left" /><span><small>PREVIOUS</small><strong>{previous.title}</strong></span></a> : <span />}
        {next ? <a href={hrefFor(next.slug)} className="pager-next"><span><small>NEXT</small><strong>{next.title}</strong></span><Icon name="arrow-right" /></a> : <span />}
      </div>
      <div className="footer-line"><span><PocketBaseMark /> OpenPocketBase SDK</span><span>Independent · Open source · MIT</span></div>
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
  const [theme, setTheme] = useState<Theme>(getInitialTheme);

  useEffect(() => applyTheme(theme), [theme]);

  useEffect(() => {
    setSidebarOpen(false);
    setActiveSection(page.sections[0]?.id ?? "");
    window.scrollTo({ top: 0 });
    document.title = `${page.title} | OpenPocketBase Docs`;
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
        if (element && element.getBoundingClientRect().top < 160) current = section.id;
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

  const toggleTheme = () => setTheme((current) => {
    const updated = nextTheme(current);
    saveTheme(updated);
    return updated;
  });

  return (
    <div className="app-shell">
      <div className="ambient-grid" />
      <div className="scroll-meter" style={{ transform: `scaleX(${scrollProgress})` }} />
      <SiteHeader activeSlug={slug} theme={theme} onMenu={() => setSidebarOpen(true)} onSearch={() => setSearchOpen(true)} onThemeToggle={toggleTheme} />
      <Sidebar activeSlug={slug} open={sidebarOpen} onClose={() => setSidebarOpen(false)} onSearch={() => setSearchOpen(true)} />
      <div className="main-shell">
        <div className="content-shell">
          <main className="doc-main">
            <PageHeader page={page} />
            {page.sections.map((section, sectionIndex) => (
              <section className="doc-section" id={`section-${section.id}`} key={section.id}>
                <div className="section-heading"><span>{String(sectionIndex + 1).padStart(2, "0")}</span><h2>{section.title}</h2></div>
                <div className="section-body">{section.blocks.map((block, blockIndex) => <RichBlock key={blockIndex} block={block} onCopied={showCopied} />)}</div>
              </section>
            ))}
            <PageFooter page={page} />
          </main>
          <OnThisPage page={page} activeSection={activeSection} />
        </div>
      </div>
      <SearchDialog open={searchOpen} onClose={() => setSearchOpen(false)} />
      <div className={`copy-toast ${toast ? "is-visible" : ""}`}><Icon name="check" /> Copied to clipboard</div>
    </div>
  );
}

export default App;
