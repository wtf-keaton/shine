#!/usr/bin/env node

import fs from 'fs';
import path from 'path';
import { fileURLToPath } from 'url';
import os from 'os';
import https from 'https';
import prompts from 'prompts';
import { blue, green, reset, bold } from 'kolorist';
import * as tar from 'tar';

// Resolve current script directory (ESM-friendly __dirname).
const __dirname = path.dirname(fileURLToPath(import.meta.url));

async function init() {
    console.log(`\n✨ Welcome to the ${bold(blue('Shine Framework'))} app generator!\n`);

    // 1) Collect project name
    const response = await prompts([
        {
            type: 'text',
            name: 'projectName',
            message: 'Project name:',
            initial: 'my-shine-app',
            validate: value => value.trim().length > 0 ? true : 'Project name cannot be empty'
        }
    ]);

    const projectName = response.projectName;
    if (!projectName) {
        console.log(reset('\n❌ Operation cancelled.'));
        return;
    }

    const targetDir = path.join(process.cwd(), projectName);
    if (fs.existsSync(targetDir)) {
        console.error(`\n❌ Directory "${projectName}" already exists!`);
        process.exit(1);
    }

    fs.mkdirSync(targetDir, { recursive: true });

    const templateDir = path.join(__dirname, 'template');
    const shineDir = path.join(targetDir, 'shine');

    function copyDir(src, dest) {
        fs.mkdirSync(dest, { recursive: true });
        const entries = fs.readdirSync(src, { withFileTypes: true });

        for (const entry of entries) {
            const srcPath = path.join(src, entry.name);
            const destPath = path.join(dest, entry.name);

            if (entry.name === 'node_modules' || entry.name === 'dist' || entry.name === 'cmake-build-debug') continue;

            if (entry.isDirectory()) {
                copyDir(srcPath, destPath);
            } else {
                let content = fs.readFileSync(srcPath, 'utf-8');

                if (entry.name === 'CMakeLists.txt' || entry.name === 'package.json') {
                    content = content.replace(/\{\{PROJECT_NAME\}\}/g, projectName);
                }

                fs.writeFileSync(destPath, content);
            }
        }
    }

    console.log(`\nCreating project in ${green(targetDir)}...`);
    copyDir(templateDir, targetDir);

    async function downloadFile(url, destPath, headers = {}) {
        await new Promise((resolve, reject) => {
            const file = fs.createWriteStream(destPath);

            https.get(url, { headers }, (res) => {
                // Handle redirects
                if (res.statusCode >= 300 && res.statusCode < 400 && res.headers.location) {
                    file.close();
                    fs.unlinkSync(destPath);
                    downloadFile(res.headers.location, destPath, headers).then(resolve, reject);
                    return;
                }

                if (res.statusCode !== 200) {
                    const hint = (res.statusCode === 401 || res.statusCode === 403)
                        ? "\nHint: set GITHUB_TOKEN (or SHINE_GITHUB_TOKEN) with access to the repository."
                        : "";
                    reject(new Error(`Failed to download ${url}. Status: ${res.statusCode}${hint}`));
                    return;
                }

                res.pipe(file);
                file.on('finish', () => file.close(resolve));
            }).on('error', (err) => {
                file.close();
                reject(err);
            });
        });
    }

    async function fetchJson(url, headers = {}) {
        return await new Promise((resolve, reject) => {
            let body = '';
            https.get(url, { headers }, (res) => {
                if (res.statusCode >= 300 && res.statusCode < 400 && res.headers.location) {
                    fetchJson(res.headers.location, headers).then(resolve, reject);
                    return;
                }
                if (res.statusCode !== 200) {
                    const hint = (res.statusCode === 401 || res.statusCode === 403)
                        ? "\nHint: set GITHUB_TOKEN (or SHINE_GITHUB_TOKEN) with access to the repository."
                        : "";
                    reject(new Error(`Failed to fetch ${url}. Status: ${res.statusCode}${hint}`));
                    return;
                }
                res.setEncoding('utf8');
                res.on('data', (chunk) => body += chunk);
                res.on('end', () => {
                    try {
                        resolve(JSON.parse(body));
                    } catch (e) {
                        reject(new Error(`Failed to parse JSON from ${url}: ${e?.message || e}`));
                    }
                });
            }).on('error', reject);
        });
    }

    function normalizeRepo(repo) {
        // Accept:
        // - https://github.com/user/repo
        // - https://github.com/user/repo.git
        // - git@github.com:user/repo.git
        let r = repo.trim();
        if (r.endsWith('.git')) r = r.slice(0, -4);
        if (r.startsWith('git@github.com:')) {
            r = 'https://github.com/' + r.slice('git@github.com:'.length);
            if (r.endsWith('.git')) r = r.slice(0, -4);
        }
        return r;
    }

    function parseGithubOwnerRepo(repoUrl) {
        const r = normalizeRepo(repoUrl);
        const m = r.match(/^https:\/\/github\.com\/([^/]+)\/([^/]+)$/i);
        if (!m) return null;
        return { owner: m[1], repo: m[2] };
    }

    async function downloadAndExtractShine() {
        const repoUrl = process.env.SHINE_REPO || 'https://github.com/wtf-keaton/shine';
        const ref = (process.env.SHINE_REF || 'develop').trim();
        const releaseTag = (process.env.SHINE_RELEASE_TAG || '').trim(); // legacy override

        // Prefer GitHub API endpoints when available (supports auth).
        // You can also override with a fully-qualified URL:
        //   SHINE_TARBALL_URL=https://.../some.tar.gz
        const explicitTarball = process.env.SHINE_TARBALL_URL && process.env.SHINE_TARBALL_URL.trim();

        const gh = parseGithubOwnerRepo(repoUrl);

        const token = (process.env.GITHUB_TOKEN || process.env.SHINE_GITHUB_TOKEN || '').trim();
        const headers = {
            'User-Agent': 'create-shine-app',
            'Accept': 'application/vnd.github+json'
        };
        if (token) {
            headers['Authorization'] = `Bearer ${token}`;
        }

        let tarballUrl = '';
        if (explicitTarball) {
            tarballUrl = explicitTarball;
        } else if (gh) {
            // Default: always use the latest GitHub Release.
            // This keeps users on the newest version without requiring env vars.
            const latest = await fetchJson(
                `https://api.github.com/repos/${gh.owner}/${gh.repo}/releases/latest`,
                headers
            );

            // If you attach a source archive asset (recommended), we prefer it.
            // Fallback: GitHub-provided tarball_url.
            const preferredAssetName = (process.env.SHINE_RELEASE_ASSET_NAME || 'shine-src.tar.gz').trim(); // optional override

            tarballUrl = latest.tarball_url;
            if (Array.isArray(latest.assets)) {
                const a = latest.assets.find(x => x && x.name === preferredAssetName);
                if (a && a.url) {
                    tarballUrl = a.url; // GitHub API asset URL (requires Accept: octet-stream)
                }
            }

            // Legacy mode: allow fetching a specific release by tag if someone needs it.
            if (releaseTag) {
                const rel = await fetchJson(
                    `https://api.github.com/repos/${gh.owner}/${gh.repo}/releases/tags/${encodeURIComponent(releaseTag)}`,
                    headers
                );
                tarballUrl = rel.tarball_url;

                const assetName = (process.env.SHINE_RELEASE_ASSET_NAME || '').trim();
                if (assetName && Array.isArray(rel.assets)) {
                    const a = rel.assets.find(x => x && x.name === assetName);
                    if (!a) {
                        throw new Error(`Release asset not found: ${assetName}`);
                    }
                    tarballUrl = a.url;
                }
            }
        } else if (releaseTag) {
            if (!gh) {
                throw new Error(`SHINE_RELEASE_TAG requires SHINE_REPO to be a GitHub repo URL (https://github.com/<owner>/<repo>). Got: ${repoUrl}`);
            }
            const release = await fetchJson(
                `https://api.github.com/repos/${gh.owner}/${gh.repo}/releases/tags/${encodeURIComponent(releaseTag)}`,
                headers
            );

            // Prefer GitHub-provided source tarball for the release tag.
            // This still works for private repos when authenticated.
            tarballUrl = release.tarball_url;

            // Optional: allow selecting a specific release asset by name.
            const assetName = (process.env.SHINE_RELEASE_ASSET_NAME || '').trim();
            if (assetName && Array.isArray(release.assets)) {
                const a = release.assets.find(x => x && x.name === assetName);
                if (!a) {
                    throw new Error(`Release asset not found: ${assetName}`);
                }
                tarballUrl = a.url; // GitHub API asset URL (requires Accept: octet-stream)
            }
        } else {
            tarballUrl = gh
                ? `https://api.github.com/repos/${gh.owner}/${gh.repo}/tarball/${encodeURIComponent(ref)}`
                : `${normalizeRepo(repoUrl)}/archive/${encodeURIComponent(ref)}.tar.gz`;
        }

        // If downloading a release asset via API "assets.url", need octet-stream accept header.
        const isGithubAssetApiUrl = tarballUrl.startsWith('https://api.github.com/') && tarballUrl.includes('/releases/assets/');
        const dlHeaders = isGithubAssetApiUrl
            ? { ...headers, 'Accept': 'application/octet-stream' }
            : headers;

        fs.mkdirSync(shineDir, { recursive: true });

        const tmpFile = path.join(os.tmpdir(), `shine-src-${Date.now()}.tar.gz`);
        const label = (releaseTag ? `release:${releaseTag}` : 'latest-release');
        console.log(`\nDownloading Shine sources (${blue(label)})...`);
        console.log(reset(`  ${tarballUrl}`));

        await downloadFile(tarballUrl, tmpFile, dlHeaders);
        console.log(`Extracting Shine into ${green(shineDir)}...`);

        // GitHub tarballs have a top-level folder; strip it.
        await tar.x({ file: tmpFile, cwd: shineDir, strip: 1 });

        try { fs.unlinkSync(tmpFile); } catch { /* ignore */ }
    }

    await downloadAndExtractShine();

    console.log(`\n🎉 Project ${blue(projectName)} created successfully!\n`);
    console.log('Next steps:');
    console.log(bold(`  cd ${projectName}`));
    console.log(bold(`  npm install`));
    console.log(bold(`  npm run dev`));
    console.log(`\nThen open ${bold('CMakeLists.txt')} in CLion or Visual Studio and run the native target.\n`);
}

init().catch((e) => {
    console.error(e);
});