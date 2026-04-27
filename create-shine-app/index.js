#!/usr/bin/env node

import fs from 'fs';
import path from 'path';
import { fileURLToPath } from 'url';
import os from 'os';
import https from 'https';
import prompts from 'prompts';
import { blue, green, reset, bold } from 'kolorist';
import AdmZip from 'adm-zip';

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
                    reject(new Error(`Failed to download ${url}. Status: ${res.statusCode}`));
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

    function listDirSafe(dir) {
        try {
            return fs.readdirSync(dir, { withFileTypes: true });
        } catch {
            return [];
        }
    }

    function removeDirRecursive(dir) {
        try {
            fs.rmSync(dir, { recursive: true, force: true });
        } catch {
            // ignore
        }
    }

    function moveDirContentsUp(oneChildDir, target) {
        const entries = listDirSafe(oneChildDir);
        for (const e of entries) {
            const from = path.join(oneChildDir, e.name);
            const to = path.join(target, e.name);
            if (fs.existsSync(to)) {
                throw new Error(`Extraction produced an unexpected existing path: ${to}`);
            }
            fs.renameSync(from, to);
        }
        removeDirRecursive(oneChildDir);
    }

    async function downloadAndExtractShine() {
        const DEFAULT_ZIP_URL = 'https://github.com/wtf-keaton/shine/releases/download/v1.0.0/shine-framework-v1.0.0.zip';
        const zipUrl = (process.env.SHINE_ZIP_URL || DEFAULT_ZIP_URL).trim();

        const headers = {
            'User-Agent': 'create-shine-app',
            'Accept': 'application/octet-stream'
        };

        fs.mkdirSync(shineDir, { recursive: true });

        const tmpZip = path.join(os.tmpdir(), `shine-framework-${Date.now()}.zip`);
        console.log(`\nDownloading Shine framework archive...`);
        console.log(reset(`  ${zipUrl}`));

        await downloadFile(zipUrl, tmpZip, headers);
        console.log(`Extracting Shine into ${green(shineDir)}...`);

        const zip = new AdmZip(tmpZip);
        zip.extractAllTo(shineDir, true);

        // If the zip contains a single top-level folder (common for release zips),
        // move its contents up into shineDir to keep the expected layout.
        const top = listDirSafe(shineDir)
            .filter(e => e.name !== '__MACOSX' && e.name !== '.DS_Store');

        if (top.length === 1 && top[0].isDirectory()) {
            const wrapper = path.join(shineDir, top[0].name);
            moveDirContentsUp(wrapper, shineDir);
            removeDirRecursive(path.join(shineDir, '__MACOSX'));
        }

        try { fs.unlinkSync(tmpZip); } catch { /* ignore */ }
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