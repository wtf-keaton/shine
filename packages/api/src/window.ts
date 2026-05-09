import {invoke} from './core.js';

export const appWindow = {
    startDrag: async (): Promise<void> => {
        await invoke<void>('window_drag');
    },
    close: async (): Promise<void> => {
        await invoke<void>('close');
    },
    minimize: async (): Promise<void> => {
        await invoke<void>('minimize');
    },
    maximize: async (): Promise<void> => {
        await invoke<void>('maximize');
    }
};