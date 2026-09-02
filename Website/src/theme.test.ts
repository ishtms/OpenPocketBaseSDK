import { describe, expect, it } from "vitest";
import { nextTheme, resolveTheme, themeColor } from "./theme";

describe("site theme", () => {
  it("uses a saved theme before the system preference", () => {
    expect(resolveTheme("light", false)).toBe("light");
    expect(resolveTheme("dark", true)).toBe("dark");
  });

  it("uses the system preference when there is no saved choice", () => {
    expect(resolveTheme(null, true)).toBe("light");
    expect(resolveTheme(null, false)).toBe("dark");
    expect(resolveTheme("unknown", true)).toBe("light");
  });

  it("toggles between dark and light themes", () => {
    expect(nextTheme("dark")).toBe("light");
    expect(nextTheme("light")).toBe("dark");
  });

  it("provides matching browser chrome colors", () => {
    expect(themeColor.dark).toBe("#07100f");
    expect(themeColor.light).toBe("#f7faf9");
  });
});
