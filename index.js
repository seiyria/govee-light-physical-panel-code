
const { getDevices, multiDeviceControl } = require('./govee.js');
const { buttonConfigs } = require('./buttons.js');

function getRandomButtonConfig() {
  return buttonConfigs[Math.floor(Math.random() * buttonConfigs.length)];
}

async function eventLoop() {
  const controllableDevices = await getDevices();

  console.info(`Controllable devices for this instance: ${controllableDevices.map(device => device.deviceName).join(', ')}`);
  console.info(`Total controllable devices for this instance: ${controllableDevices.length}`);

  console.log(controllableDevices)
  
  // const configSequence = getRandomButtonConfig();

  // multiDeviceControl(controllableDevices, configSequence);
}

eventLoop();