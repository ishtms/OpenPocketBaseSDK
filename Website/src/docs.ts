import { authPages } from "./content/auth";
import { corePages } from "./content/core";
import { dataPages } from "./content/data";
import { operationPages } from "./content/operations";
import { referencePages } from "./content/reference";
import { startPages } from "./content/start";
import { tutorialPages } from "./content/tutorials";
import type { DocBlock, DocPage, NavGroup } from "./content/types";

export type { DocBlock, DocPage, DocSection, NavGroup } from "./content/types";

export const docs: DocPage[] = [
  ...startPages,
  ...tutorialPages,
  ...corePages,
  ...dataPages,
  ...authPages,
  ...operationPages,
  ...referencePages,
];

export const navigation: NavGroup[] = [
  {
    title: "Start here",
    items: [
      { slug: "overview", title: "Overview" },
      { slug: "start/installation", title: "Installation" },
      { slug: "start/blueprint-basics", title: "Blueprint mental model" },
      { slug: "start/cpp-basics", title: "C++ mental model" },
    ],
  },
  {
    title: "Blueprint tutorials",
    items: [
      { slug: "tutorials/getting-started", title: "1. Getting started" },
      { slug: "tutorials/connection-health", title: "2. Connection & health" },
      { slug: "tutorials/create-record", title: "3. Create a record" },
      { slug: "tutorials/read-records", title: "4. Read records" },
      { slug: "tutorials/update-record", title: "5. Update a record" },
      { slug: "tutorials/delete-record", title: "6. Delete a record" },
      { slug: "tutorials/query-records", title: "7. Filter, sort & page" },
      { slug: "tutorials/authentication", title: "8. Authentication" },
      { slug: "tutorials/realtime", title: "9. Realtime" },
    ],
  },
  {
    title: "Core SDK",
    items: [
      { slug: "core/configuration", title: "Configuration & profiles" },
      { slug: "core/client-lifecycle", title: "Client lifecycle" },
      { slug: "core/requests-errors", title: "Requests & errors" },
      { slug: "core/capabilities", title: "Capabilities" },
    ],
  },
  {
    title: "Records",
    items: [
      { slug: "records/crud", title: "Create, read, update, delete" },
      { slug: "records/reading-fields", title: "Reading fields" },
      { slug: "records/filters", title: "Filters" },
      { slug: "records/pagination", title: "Pagination & full lists" },
      { slug: "records/batches", title: "Transactional batches" },
    ],
  },
  {
    title: "Authentication",
    items: [
      { slug: "authentication/otp-mfa", title: "OTP and MFA" },
      { slug: "authentication/oauth2", title: "OAuth2" },
      { slug: "authentication/account", title: "Account actions" },
      { slug: "authentication/session", title: "Session & refresh" },
      { slug: "authentication/persistence", title: "Secure persistence" },
    ],
  },
  {
    title: "Files & realtime",
    items: [
      { slug: "files/uploads", title: "File uploads" },
      { slug: "files/downloads", title: "URLs & downloads" },
      { slug: "realtime/subscriptions", title: "Realtime subscriptions" },
    ],
  },
  {
    title: "Tools & production",
    items: [
      { slug: "tools/custom-routes", title: "Custom routes & health" },
      { slug: "tools/admin-api", title: "Privileged admin API" },
      { slug: "tools/security", title: "Security model" },
      { slug: "tools/platform-support", title: "Platform support" },
      { slug: "tools/testing", title: "Testing" },
    ],
  },
  {
    title: "Reference",
    items: [
      { slug: "reference/feature-status", title: "Feature status" },
      { slug: "reference/blueprint-nodes", title: "Blueprint node index" },
      { slug: "reference/data-types", title: "Data types" },
      { slug: "reference/api-index", title: "C++ API index" },
    ],
  },
];

export const docsBySlug = new Map(docs.map((page) => [page.slug, page]));

const blockText = (block: DocBlock): string => {
  switch (block.type) {
    case "paragraph":
    case "lead":
    case "screenshot":
      return block.text;
    case "bullets":
      return block.items.join(" ");
    case "steps":
      return block.items.map((item) => `${item.title} ${item.text}`).join(" ");
    case "code":
      return `${block.label ?? ""} ${block.code}`;
    case "callout":
      return `${block.title} ${block.text}`;
    case "table":
      return `${block.columns.join(" ")} ${block.rows.flat().join(" ")}`;
    case "cards":
      return block.items.map((item) => `${item.title} ${item.text} ${item.label ?? ""}`).join(" ");
  }
};

export const pageText = (page: DocPage): string =>
  [
    page.title,
    page.eyebrow,
    page.description,
    ...page.sections.flatMap((section) => [section.title, ...section.blocks.map(blockText)]),
  ]
    .join(" ")
    .toLowerCase();

export type SearchResult = {
  slug: string;
  title: string;
  description: string;
  eyebrow: string;
  score: number;
};

export const searchDocs = (query: string): SearchResult[] => {
  const terms = query.toLowerCase().trim().split(/\s+/).filter(Boolean);
  if (terms.length === 0) {
    return docs.slice(0, 8).map((page, index) => ({ ...page, score: 8 - index }));
  }

  return docs
    .map((page) => {
      const title = page.title.toLowerCase();
      const description = page.description.toLowerCase();
      const text = pageText(page);
      const score = terms.reduce((total, term) => {
        if (!text.includes(term)) return total - 100;
        if (title.includes(term)) return total + 12;
        if (description.includes(term)) return total + 6;
        return total + 2;
      }, 0);

      return { slug: page.slug, title: page.title, description: page.description, eyebrow: page.eyebrow, score };
    })
    .filter((result) => result.score > -50)
    .sort((a, b) => b.score - a.score || a.title.localeCompare(b.title))
    .slice(0, 12);
};
