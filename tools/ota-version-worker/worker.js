// Cloudflare Worker: tells the weather stations the latest firmware version
// over plain HTTP. GitHub is HTTPS-only, and TLS doesn't fit next to the
// running UI on the ESP8266; this answer needs no trust -- a wrong version
// only makes a unit reboot into update mode, which verifies the signed release.
//
// GET /latest -> "1.2.0\n" (latest release tag without its "v"), 502 if the
// repo has no release.

const REPO = "pavels/tft-weather-station";

export default {
  async fetch(request) {
    const url = new URL(request.url);
    if (url.pathname !== "/latest") {
      return new Response("not found\n", { status: 404 });
    }

    // github.com/<repo>/releases/latest redirects to .../releases/tag/<tag>.
    const github = await fetch(`https://github.com/${REPO}/releases/latest`, {
      redirect: "manual",
      headers: { "User-Agent": "tft-weather-station-version-worker" },
    });
    const location = github.headers.get("Location") || "";
    const tag = location.match(/\/releases\/tag\/v?([^/?#]+)$/);
    if (!tag) {
      return new Response("no release\n", { status: 502 });
    }

    return new Response(`${tag[1]}\n`, {
      headers: { "Content-Type": "text/plain", "Cache-Control": "no-store" },
    });
  },
};
