#!/usr/bin/env node

import fs from 'fs';
import path from 'path';
import { fileURLToPath } from 'url';
import prompts from 'prompts';
import { blue, green, reset, bold } from 'kolorist';

// Получаем текущую директорию скрипта (в ES-модулях __dirname не работает из коробки)
const __dirname = path.dirname(fileURLToPath(import.meta.url));

async function init() {
    console.log(`\n✨ Добро пожаловать в генератор ${bold(blue('Shine Framework'))}!\n`);

    // 1. Задаем вопросы пользователю
    const response = await prompts([
        {
            type: 'text',
            name: 'projectName',
            message: 'Имя проекта:',
            initial: 'my-shine-app',
            validate: value => value.trim().length > 0 ? true : 'Имя не может быть пустым'
        }
    ]);

    const projectName = response.projectName;
    if (!projectName) {
        console.log(reset('\n❌ Операция отменена.'));
        return;
    }

    const targetDir = path.join(process.cwd(), projectName);
    if (fs.existsSync(targetDir)) {
        console.error(`\n❌ Папка "${projectName}" уже существует!`);
        process.exit(1);
    }

    fs.mkdirSync(targetDir, { recursive: true });

    const templateDir = path.join(__dirname, 'template');

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

    console.log(`\nСоздание проекта в ${green(targetDir)}...`);
    copyDir(templateDir, targetDir);

    console.log(`\n🎉 Проект ${blue(projectName)} успешно создан!\n`);
    console.log('Следующие шаги:');
    console.log(bold(`  cd ${projectName}/frontend`));
    console.log(bold(`  npm install`));
    console.log(bold(`  npm run dev`));
    console.log(`\nА затем откройте ${bold('CMakeLists.txt')} в CLion или Visual Studio и запустите проект!\n`);
}

init().catch((e) => {
    console.error(e);
});