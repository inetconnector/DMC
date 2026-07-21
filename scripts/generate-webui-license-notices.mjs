import { createHash } from "node:crypto";
import { existsSync, readdirSync, readFileSync, writeFileSync } from "node:fs";
import { dirname, join, resolve } from "node:path";
import { fileURLToPath } from "node:url";

const scriptDir = dirname(fileURLToPath(import.meta.url));
const repositoryRoot = resolve(scriptDir, "..");
const uiRoot = join(repositoryRoot, "upstream", "llama.cpp", "tools", "ui");
const lockPath = join(uiRoot, "package-lock.json");
const outputPath = join(repositoryRoot, "third_party_licenses", "WEB_UI_NOTICES.txt");

if (!existsSync(lockPath)) {
  throw new Error(`Missing Web UI lockfile: ${lockPath}`);
}

const lock = JSON.parse(readFileSync(lockPath, "utf8"));
const groups = new Map();
const missing = [];
let installedPackages = 0;

for (const [packagePath, lockEntry] of Object.entries(lock.packages ?? {})) {
  if (!packagePath.includes("node_modules/")) continue;

  const packageDir = join(uiRoot, ...packagePath.split("/"));
  const packageJsonPath = join(packageDir, "package.json");
  if (!existsSync(packageJsonPath)) continue; // Platform-specific optional package.

  installedPackages += 1;
  const packageJson = JSON.parse(readFileSync(packageJsonPath, "utf8"));
  const name = packageJson.name ?? packagePath.slice(packagePath.lastIndexOf("node_modules/") + 13);
  const version = packageJson.version ?? lockEntry.version ?? "unknown";
  const licenseExpression = packageJson.license ?? lockEntry.license ?? "UNKNOWN";
  const label = `${name}@${version} (${licenseExpression})`;
  const licenseFile = readdirSync(packageDir, { withFileTypes: true })
    .filter((entry) => entry.isFile())
    .map((entry) => entry.name)
    .sort()
    .find((entry) => /^(licen[cs]e|copying|notice)(\..*)?$/i.test(entry));

  if (!licenseFile) {
    missing.push(label);
    continue;
  }

  const licenseText = readFileSync(join(packageDir, licenseFile), "utf8")
    .split(/\r?\n/)
    .map((line) => line.trimEnd())
    .join("\n")
    .trim();
  const hash = createHash("sha256").update(licenseText).digest("hex");
  const group = groups.get(hash) ?? { packages: [], text: licenseText };
  group.packages.push(label);
  groups.set(hash, group);
}

const lines = [
  "DMC Web UI third-party license notices",
  "======================================",
  "",
  "Generated from upstream/llama.cpp/tools/ui/package-lock.json and the installed",
  "node_modules tree. The listed packages keep their respective licenses; this",
  "file does not relicense them under the DMC MIT license.",
  "",
  `Installed packages inspected: ${installedPackages}`,
  `Distinct license texts: ${groups.size}`,
  `Packages without a local license file: ${missing.length}`,
  "",
];

if (missing.length > 0) {
  lines.push("PACKAGES WITHOUT A LOCAL LICENSE FILE", "-------------------------------------");
  lines.push(...missing.sort(), "");
}

for (const [hash, group] of [...groups.entries()].sort((a, b) =>
  a[1].packages[0].localeCompare(b[1].packages[0]),
)) {
  lines.push("======================================================================");
  lines.push(`License text SHA-256: ${hash}`);
  lines.push("Applies to:");
  lines.push(...group.packages.sort().map((name) => `- ${name}`));
  lines.push("", group.text, "");
}

writeFileSync(outputPath, `${lines.join("\n").trimEnd()}\n`, "utf8");
console.log(`Wrote ${outputPath}`);
console.log(`Inspected ${installedPackages} packages and found ${groups.size} license texts.`);
