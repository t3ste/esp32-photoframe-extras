// The links the Updates tab shows for the repository the frame takes its updates from (build option `fixes`).
//
// The frame knows its update feed (GET /api/ota/status: "repo": "owner/name"); the page links to that repository. Only some
// repositories have a wiki: the extended edition has one, the base fork and its mirror do not, so a repository is listed in
// WIKI_REPOS when its wiki is worth a link.
export const WIKI_REPOS = ["t3ste/esp32-photoframe-extras"];

const REPO = /^[A-Za-z0-9_.-]+\/[A-Za-z0-9_.-]+$/;

// { name, url, wikiUrl } for an "owner/name" answer of the frame, null for anything else (an older firmware sends none)
export function repoLinks(repo, wikiRepos = WIKI_REPOS) {
  if (typeof repo !== "string" || !REPO.test(repo)) return null;
  const url = `https://github.com/${repo}`;
  return { name: repo, url, wikiUrl: wikiRepos.includes(repo) ? `${url}/wiki` : null };
}
