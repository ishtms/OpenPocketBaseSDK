import { describe, expect, it } from "vitest";
import { docs, navigation, pageText, searchDocs } from "./docs";

describe("documentation registry", () => {
  it("uses unique slugs and resolves every navigation item", () => {
    const slugs = docs.map((page) => page.slug);
    expect(new Set(slugs).size).toBe(slugs.length);

    const knownSlugs = new Set(slugs);
    for (const group of navigation) {
      for (const item of group.items) {
        expect(knownSlugs.has(item.slug), item.slug).toBe(true);
      }
    }

    for (const page of docs) {
      const sectionIds = page.sections.map((section) => section.id);
      expect(new Set(sectionIds).size, page.slug).toBe(sectionIds.length);

      for (const block of page.sections.flatMap((section) => section.blocks)) {
        if (block.type === "cards") {
          for (const card of block.items) {
            if (card.link) expect(knownSlugs.has(card.link), card.link).toBe(true);
          }
        }
      }
    }
  });

  it("indexes the major SDK workflows", () => {
    expect(searchDocs("oauth").some((result) => result.slug === "authentication/oauth2")).toBe(true);
    expect(searchDocs("bounded full list").some((result) => result.slug === "records/pagination")).toBe(true);
    expect(searchDocs("protected file token").some((result) => result.slug === "files/downloads")).toBe(true);
    expect(searchDocs("privileged policy").some((result) => result.slug === "tools/admin-api")).toBe(true);
  });

  it("starts with the overview and ends with the API index", () => {
    expect(docs.at(0)?.slug).toBe("overview");
    expect(docs.at(-1)?.slug).toBe("reference/api-index");
  });

  it("keeps screenshot requests explicit and replaceable", () => {
    const placeholders = docs.flatMap((page) =>
      page.sections.flatMap((section) =>
        section.blocks.filter((block) => block.type === "screenshot")
      )
    );

    expect(placeholders.length).toBeGreaterThan(45);
    for (const placeholder of placeholders) {
      if (placeholder.type === "screenshot") {
        expect(placeholder.text.startsWith("[put a screenshot of ")).toBe(true);
        expect(placeholder.text.endsWith(" here]")).toBe(true);
      }
    }
  });

  it("keeps the Blueprint tutorial course ordered and capture-rich", () => {
    const tutorials = docs.filter((page) => page.tutorial);

    expect(tutorials.map((page) => page.tutorial?.order)).toEqual([1, 2, 3, 4, 5, 6, 7, 8, 9]);
    for (const tutorial of tutorials) {
      const captures = tutorial.sections.flatMap((section) =>
        section.blocks.filter((block) => block.type === "screenshot")
      );
      expect(captures.length, tutorial.slug).toBeGreaterThanOrEqual(4);
      expect(tutorial.tutorial?.outcome.length, tutorial.slug).toBeGreaterThan(20);
      expect(tutorial.tutorial?.prerequisites.length, tutorial.slug).toBeGreaterThan(0);
    }
  });

  it("does not publish internal fixture walkthroughs", () => {
    const publishedText = docs.map(pageText).join(" ");
    expect(publishedText).not.toContain("pb_testing");
    expect(publishedText).not.toContain("bp_pb_chunk");
  });
});
