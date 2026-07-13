import type { ReactNode, SVGProps } from "react";

export type IconName =
  | "arrow-left"
  | "arrow-right"
  | "book"
  | "check"
  | "chevron-down"
  | "chevron-right"
  | "clipboard"
  | "code"
  | "command"
  | "database"
  | "external"
  | "file"
  | "github"
  | "grid"
  | "haptics"
  | "info"
  | "menu"
  | "mobile"
  | "radio"
  | "search"
  | "sensor"
  | "shield"
  | "spark"
  | "terminal"
  | "x"
  | "zap";

const paths: Record<IconName, ReactNode> = {
  "arrow-left": <><path d="m15 18-6-6 6-6"/><path d="M21 12H9"/></>,
  "arrow-right": <><path d="m9 18 6-6-6-6"/><path d="M3 12h12"/></>,
  book: <><path d="M4 19.5A2.5 2.5 0 0 1 6.5 17H20"/><path d="M6.5 2H20v20H6.5A2.5 2.5 0 0 1 4 19.5v-15A2.5 2.5 0 0 1 6.5 2Z"/></>,
  check: <path d="m5 12 4 4L19 6"/>,
  "chevron-down": <path d="m6 9 6 6 6-6"/>,
  "chevron-right": <path d="m9 18 6-6-6-6"/>,
  clipboard: <><rect x="8" y="4" width="12" height="16"/><path d="M16 4V2H4v14h4"/></>,
  code: <><path d="m8 9-4 3 4 3"/><path d="m16 9 4 3-4 3"/><path d="m14 5-4 14"/></>,
  command: <><path d="M18 9a3 3 0 1 0-3-3v12a3 3 0 1 0 3-3H6a3 3 0 1 0 3 3V6a3 3 0 1 0-3 3Z"/></>,
  database: <><ellipse cx="12" cy="5" rx="8" ry="3"/><path d="M4 5v14c0 1.7 3.6 3 8 3s8-1.3 8-3V5"/><path d="M4 12c0 1.7 3.6 3 8 3s8-1.3 8-3"/></>,
  external: <><path d="M15 3h6v6"/><path d="m10 14 11-11"/><path d="M18 13v7H4V6h7"/></>,
  file: <><path d="M14 2H6v20h12V6Z"/><path d="M14 2v4h4"/><path d="M9 13h6M9 17h6"/></>,
  github: <path d="M15 22v-4c.1-1-.4-1.8-1-2.2 3.3-.4 6.8-1.6 6.8-7.4 0-1.6-.6-3-1.6-4 .2-.4.7-2-.2-4-1.3-.4-4.2 1.6-4.2 1.6A14.5 14.5 0 0 0 9 2S6.1 0 4.8.4c-.9 2-.4 3.6-.2 4a5.8 5.8 0 0 0-1.6 4c0 5.8 3.5 7 6.8 7.4-.4.4-.8 1-.9 2.1-.8.4-2.9 1-4.2-1.2-.8-1.3-2.2-1.4-2.2-1.4"/>,
  grid: <><rect x="3" y="3" width="7" height="7"/><rect x="14" y="3" width="7" height="7"/><rect x="3" y="14" width="7" height="7"/><rect x="14" y="14" width="7" height="7"/></>,
  haptics: <><path d="M9 18V5l10-2v13"/><circle cx="6" cy="18" r="3"/><circle cx="16" cy="16" r="3"/></>,
  info: <><circle cx="12" cy="12" r="9"/><path d="M12 11v6M12 7h.01"/></>,
  menu: <><path d="M4 7h16M4 12h16M4 17h16"/></>,
  mobile: <><rect x="7" y="2" width="10" height="20"/><path d="M11 18h2"/></>,
  radio: <><path d="M5.6 18.4a9 9 0 0 1 0-12.8M9.2 14.8a4 4 0 0 1 0-5.6M18.4 5.6a9 9 0 0 1 0 12.8M14.8 9.2a4 4 0 0 1 0 5.6"/><circle cx="12" cy="12" r="1"/></>,
  search: <><circle cx="10.5" cy="10.5" r="6.5"/><path d="m16 16 5 5"/></>,
  sensor: <><circle cx="12" cy="12" r="2"/><path d="M12 2v3M12 19v3M2 12h3M19 12h3M4.9 4.9 7 7M17 17l2.1 2.1M19.1 4.9 17 7M7 17l-2.1 2.1"/></>,
  shield: <path d="M12 3 4 6v5c0 5 3.4 8.4 8 10 4.6-1.6 8-5 8-10V6Z"/>,
  spark: <path d="m12 2 1.6 5.4L19 9l-5.4 1.6L12 16l-1.6-5.4L5 9l5.4-1.6ZM19 16l.7 2.3L22 19l-2.3.7L19 22l-.7-2.3L16 19l2.3-.7Z"/>,
  terminal: <><path d="m4 7 5 5-5 5"/><path d="M12 17h8"/></>,
  x: <><path d="m6 6 12 12M18 6 6 18"/></>,
  zap: <path d="M13 2 4 14h7l-1 8 9-12h-7Z"/>,
};

export function Icon({ name, ...props }: { name: IconName } & SVGProps<SVGSVGElement>) {
  return (
    <svg
      viewBox="0 0 24 24"
      fill="none"
      stroke="currentColor"
      strokeWidth="1.7"
      strokeLinecap="square"
      strokeLinejoin="miter"
      aria-hidden="true"
      {...props}
    >
      {paths[name]}
    </svg>
  );
}

export function OpenMobileMark({ className }: { className?: string }) {
  return (
    <svg className={className} viewBox="0 0 40 40" fill="none" aria-hidden="true">
      <path d="M20 2c5.2 0 8.2 6 5 10.2L20 18.5 15 12.2C11.8 8 14.8 2 20 2Z" fill="currentColor" />
      <path d="M38 20c0 5.2-6 8.2-10.2 5L21.5 20l6.3-5C32 11.8 38 14.8 38 20Z" fill="currentColor" />
      <path d="M20 38c-5.2 0-8.2-6-5-10.2l5-6.3 5 6.3C28.2 32 25.2 38 20 38Z" fill="currentColor" />
      <path d="M2 20c0-5.2 6-8.2 10.2-5l6.3 5-6.3 5C8 28.2 2 25.2 2 20Z" fill="currentColor" />
    </svg>
  );
}
