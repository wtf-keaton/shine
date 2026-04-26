import { invoke } from './core.js';
export async function readTextFile(path) {
    const response = await invoke('fs_read_text_file', { path });
    return response.content;
}
