import { registerPlugin } from '@capacitor/core';

import type { BlePeripheralPlugin } from './definitions';

const BlePeripheral = registerPlugin<BlePeripheralPlugin>('BlePeripheral', {
  web: () => import('./web').then(m => new m.BlePeripheralWeb()),
});

export * from './definitions';
export { BlePeripheral };
