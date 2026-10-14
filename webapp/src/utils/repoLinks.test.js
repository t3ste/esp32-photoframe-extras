import { describe, expect, it } from "vitest";
import { WIKI_REPOS, repoLinks } from "./repoLinks";

describe("repoLinks", () => {
  it("links the repository of the update feed", () => {
    expect(repoLinks("t3stier/esp32-photoframe-rebuild")).toEqual({
      name: "t3stier/esp32-photoframe-rebuild",
      url: "https://github.com/t3stier/esp32-photoframe-rebuild",
      wikiUrl: null,
    });
  });

  it("links a wiki only for the repositories that have one", () => {
    const wikis = ["t3ste/esp32-photoframe-extras"];
    expect(repoLinks("t3ste/esp32-photoframe-extras", wikis).wikiUrl).toBe(
      "https://github.com/t3ste/esp32-photoframe-extras/wiki"
    );
    expect(repoLinks("t3stier/esp32-photoframe-rebuild", wikis).wikiUrl).toBeNull();
    expect(repoLinks("t3ste/esp32-photoframe-rebuild", wikis).wikiUrl).toBeNull();
  });

  it("lists no wiki until one is published", () => {
    expect(WIKI_REPOS).toEqual([]);
    expect(repoLinks("t3ste/esp32-photoframe-extras").wikiUrl).toBeNull();
  });

  it("takes only what looks like owner/name", () => {
    expect(repoLinks(undefined)).toBeNull();
    expect(repoLinks("")).toBeNull();
    expect(repoLinks("no-slash")).toBeNull();
    expect(repoLinks("a/b/c")).toBeNull();
    expect(repoLinks('x/"><script>')).toBeNull();
    expect(repoLinks("javascript:alert(1)//x")).toBeNull();
  });
});
