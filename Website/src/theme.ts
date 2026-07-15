export type Theme = "dark" | "light";

export const themeStorageKey = "openmobile-theme";

export const themeColor: Record<Theme, string> = {
  dark: "#090909",
  light: "#f5f2ed",
};

const isTheme = (value: string | null | undefined): value is Theme => value === "dark" || value === "light";

export const resolveTheme = (savedTheme: string | null, prefersLight: boolean): Theme => {
  if (isTheme(savedTheme)) return savedTheme;
  return prefersLight ? "light" : "dark";
};

export const nextTheme = (theme: Theme): Theme => theme === "dark" ? "light" : "dark";

export const getInitialTheme = (): Theme => {
  const documentTheme = document.documentElement.dataset.theme;
  if (isTheme(documentTheme)) return documentTheme;

  let savedTheme: string | null = null;
  try {
    savedTheme = window.localStorage.getItem(themeStorageKey);
  } catch {
    savedTheme = null;
  }

  return resolveTheme(savedTheme, window.matchMedia("(prefers-color-scheme: light)").matches);
};

export const applyTheme = (theme: Theme) => {
  document.documentElement.dataset.theme = theme;
  document.documentElement.style.colorScheme = theme;
  document.querySelector('meta[name="theme-color"]')?.setAttribute("content", themeColor[theme]);
};

export const saveTheme = (theme: Theme) => {
  try {
    window.localStorage.setItem(themeStorageKey, theme);
  } catch {
    return;
  }
};
