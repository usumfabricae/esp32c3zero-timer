export interface BlePeripheralPlugin {
  /**
   * Initialize the BLE peripheral
   */
  initialize(): Promise<void>;

  /**
   * Start advertising as a BLE peripheral
   */
  startAdvertising(options: AdvertisingOptions): Promise<void>;

  /**
   * Stop advertising
   */
  stopAdvertising(): Promise<void>;

  /**
   * Add a GATT service
   */
  addService(options: ServiceOptions): Promise<{ serviceId: string }>;

  /**
   * Add a characteristic to a service
   */
  addCharacteristic(options: CharacteristicOptions): Promise<{ characteristicId: string }>;

  /**
   * Start the GATT server (call after adding all services and characteristics)
   */
  startServer(): Promise<void>;

  /**
   * Update characteristic value
   */
  updateCharacteristicValue(options: UpdateValueOptions): Promise<void>;

  /**
   * Send notification to connected client
   */
  sendNotification(options: NotificationOptions): Promise<void>;

  /**
   * Remove a service
   */
  removeService(options: { serviceId: string }): Promise<void>;

  /**
   * Get connection state
   */
  getConnectionState(): Promise<{ isConnected: boolean; deviceAddress?: string }>;

  /**
   * Listen for characteristic write events
   */
  addListener(
    eventName: 'characteristicWrite',
    listenerFunc: (data: CharacteristicWriteEvent) => void,
  ): Promise<PluginListenerHandle>;

  /**
   * Listen for characteristic read events
   */
  addListener(
    eventName: 'characteristicRead',
    listenerFunc: (data: CharacteristicReadEvent) => void,
  ): Promise<PluginListenerHandle>;

  /**
   * Listen for connection state changes
   */
  addListener(
    eventName: 'connectionStateChange',
    listenerFunc: (data: ConnectionStateEvent) => void,
  ): Promise<PluginListenerHandle>;

  /**
   * Remove all listeners
   */
  removeAllListeners(): Promise<void>;
}

export interface AdvertisingOptions {
  /**
   * Device name to advertise
   */
  name: string;

  /**
   * Service UUIDs to include in advertisement
   */
  serviceUuids?: string[];

  /**
   * Whether the device is connectable
   */
  connectable?: boolean;

  /**
   * Advertising timeout in milliseconds (0 = no timeout)
   */
  timeout?: number;
}

export interface ServiceOptions {
  /**
   * Service UUID (16-bit, 32-bit, or 128-bit)
   */
  uuid: string;

  /**
   * Whether this is a primary service
   */
  primary?: boolean;
}

export interface CharacteristicOptions {
  /**
   * Service UUID this characteristic belongs to
   */
  serviceUuid: string;

  /**
   * Characteristic UUID
   */
  uuid: string;

  /**
   * Characteristic properties (bit flags)
   * READ = 0x02, WRITE = 0x08, WRITE_NO_RESPONSE = 0x04, NOTIFY = 0x10, INDICATE = 0x20
   */
  properties: number;

  /**
   * Characteristic permissions (bit flags)
   * READ = 0x01, WRITE = 0x10
   */
  permissions: number;

  /**
   * Initial value (base64 encoded)
   */
  value?: string;
}

export interface UpdateValueOptions {
  /**
   * Characteristic UUID
   */
  characteristicUuid: string;

  /**
   * New value (base64 encoded)
   */
  value: string;
}

export interface NotificationOptions {
  /**
   * Characteristic UUID
   */
  characteristicUuid: string;

  /**
   * Value to send (base64 encoded)
   */
  value: string;

  /**
   * Device address to send to (optional, sends to all if not specified)
   */
  deviceAddress?: string;
}

export interface CharacteristicWriteEvent {
  serviceUuid: string;
  characteristicUuid: string;
  value: string; // base64 encoded
  deviceAddress: string;
}

export interface CharacteristicReadEvent {
  serviceUuid: string;
  characteristicUuid: string;
  deviceAddress: string;
}

export interface ConnectionStateEvent {
  isConnected: boolean;
  deviceAddress?: string;
}

export interface PluginListenerHandle {
  remove: () => Promise<void>;
}

// Property and Permission constants
export const BleProperty = {
  READ: 0x02,
  WRITE: 0x08,
  WRITE_NO_RESPONSE: 0x04,
  NOTIFY: 0x10,
  INDICATE: 0x20,
} as const;

export const BlePermission = {
  READ: 0x01,
  WRITE: 0x10,
} as const;
