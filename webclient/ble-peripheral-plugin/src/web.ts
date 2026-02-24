import { WebPlugin } from '@capacitor/core';

import type {
  BlePeripheralPlugin,
  AdvertisingOptions,
  ServiceOptions,
  CharacteristicOptions,
  UpdateValueOptions,
  NotificationOptions,
} from './definitions';

export class BlePeripheralWeb extends WebPlugin implements BlePeripheralPlugin {
  async initialize(): Promise<void> {
    throw this.unimplemented('BLE Peripheral is not supported on web platform');
  }

  async startAdvertising(_options: AdvertisingOptions): Promise<void> {
    throw this.unimplemented('BLE Peripheral is not supported on web platform');
  }

  async stopAdvertising(): Promise<void> {
    throw this.unimplemented('BLE Peripheral is not supported on web platform');
  }

  async addService(_options: ServiceOptions): Promise<{ serviceId: string }> {
    throw this.unimplemented('BLE Peripheral is not supported on web platform');
  }

  async addCharacteristic(_options: CharacteristicOptions): Promise<{ characteristicId: string }> {
    throw this.unimplemented('BLE Peripheral is not supported on web platform');
  }

  async updateCharacteristicValue(_options: UpdateValueOptions): Promise<void> {
    throw this.unimplemented('BLE Peripheral is not supported on web platform');
  }

  async sendNotification(_options: NotificationOptions): Promise<void> {
    throw this.unimplemented('BLE Peripheral is not supported on web platform');
  }

  async removeService(_options: { serviceId: string }): Promise<void> {
    throw this.unimplemented('BLE Peripheral is not supported on web platform');
  }

  async getConnectionState(): Promise<{ isConnected: boolean; deviceAddress?: string }> {
    throw this.unimplemented('BLE Peripheral is not supported on web platform');
  }
}
