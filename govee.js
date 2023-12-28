
const API_BASE_URL = 'https://developer-api.govee.com/v1';
const API_KEY = process.env.API_KEY;
const DEVICE_FILTER = process.env.DEVICE_FILTER || 'basement'; // (or 'shower')

function apiUrl(path) {
  return `${API_BASE_URL}/${path}`;
}

function apiHeaders() {
  return {
    'Govee-API-Key': API_KEY
  };
}

async function getDevices() {
  const results = await fetch(apiUrl('devices'), {
    headers: {
      ...apiHeaders()
    }
  });

  const json = await results.json();
  const devices = json?.data?.devices || [];
  
  return devices.filter(device => device.controllable && device.deviceName.includes(DEVICE_FILTER));
}

async function deviceControl(device, model, cmd) {
  console.info(`Running command "${JSON.stringify(cmd)}" for device "${device}"...`);
  const res = await fetch(apiUrl('devices/control'), {
    method: 'PUT',
    headers: {
      ...apiHeaders(),
      'Content-Type': 'application/json'
    },
    body: JSON.stringify({
      device,
      model,
      cmd
    })
  });

  try {
    const json = await res.json();
    console.info(`Response: ${JSON.stringify(json)}`);
  } catch (e) {
    console.error(`Error parsing response: ${e}`);
  }
}

async function multiDeviceControl(devices, cmdSequence) {
  const name = cmdSequence.commandName;
  const sequence = cmdSequence.commandSequence;

  console.info(`Running command sequence "${name}" for ${devices.length} devices...`);

  sequence.forEach(async cmd => {
    await Promise.all(devices.map(device => deviceControl(device.device, device.model, cmd)));
  });
}

module.exports.getDevices = getDevices;
module.exports.deviceControl = deviceControl;
module.exports.multiDeviceControl = multiDeviceControl;