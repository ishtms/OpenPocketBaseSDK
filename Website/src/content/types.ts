export type CalloutTone = "note" | "warning" | "success" | "danger";

export type DocBlock =
  | { type: "paragraph"; text: string }
  | { type: "lead"; text: string }
  | { type: "bullets"; items: string[] }
  | { type: "steps"; items: { title: string; text: string }[] }
  | { type: "code"; language: string; code: string; label?: string }
  | { type: "callout"; tone: CalloutTone; title: string; text: string }
  | {
      type: "screenshot";
      text: string;
      title?: string;
      asset?: string;
      caption?: string;
    }
  | { type: "table"; columns: string[]; rows: string[][] }
  | { type: "cards"; items: { title: string; text: string; link?: string; label?: string }[] };

export type DocSection = {
  id: string;
  title: string;
  blocks: DocBlock[];
};

export type DocPage = {
  slug: string;
  title: string;
  eyebrow: string;
  description: string;
  readTime: string;
  tutorial?: {
    order: number;
    level: "Beginner" | "Intermediate" | "Advanced";
    outcome: string;
    prerequisites: string[];
  };
  sections: DocSection[];
};

export type NavGroup = {
  title: string;
  items: { slug: string; title: string }[];
};

export const paragraph = (text: string): DocBlock => ({ type: "paragraph", text });
export const lead = (text: string): DocBlock => ({ type: "lead", text });
export const bullets = (...items: string[]): DocBlock => ({ type: "bullets", items });
export const steps = (...items: { title: string; text: string }[]): DocBlock => ({ type: "steps", items });
export const code = (language: string, value: string, label?: string): DocBlock => ({
  type: "code",
  language,
  code: value.trim(),
  label,
});
export const callout = (tone: CalloutTone, title: string, text: string): DocBlock => ({
  type: "callout",
  tone,
  title,
  text,
});
export const screenshot = (
  text: string,
  options: { title?: string; asset?: string; caption?: string } = {}
): DocBlock => ({ type: "screenshot", text, ...options });
export const table = (columns: string[], rows: string[][]): DocBlock => ({ type: "table", columns, rows });
export const cards = (...items: { title: string; text: string; link?: string; label?: string }[]): DocBlock => ({
  type: "cards",
  items,
});
