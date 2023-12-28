
module.exports.buttonConfigs = [

  // top row - temperature control
  {
    commandName: 'Dim',
    commandSequence: [{
      name: 'colorTem',
      value: 2700
    }]
  },
  {
    commandName: 'Low',
    commandSequence: [{
      name: 'colorTem',
      value: 4000
    }]
  },
  {
    commandName: 'Medium',
    commandSequence: [{
      name: 'colorTem',
      value: 5200
    }]
  },
  {
    commandName: 'Bright',
    commandSequence: [{
      name: 'colorTem',
      value: 6500
    }]
  },

  // second row - brightness control
  {
    commandName: '25%',
    commandSequence: [{
      name: 'brightness',
      value: 25
    }]
  },

  {
    commandName: '50%',
    commandSequence: [{
      name: 'brightness',
      value: 50
    }]
  },

  {
    commandName: '75%',
    commandSequence: [{
      name: 'brightness',
      value: 75
    }]
  },

  {
    commandName: '100%',
    commandSequence: [{
      name: 'brightness',
      value: 100
    }]
  },

  // third row
  {
    commandName: 'Red',
    commandSequence: [{
      name: 'color',
      value: {
        r: 255,
        g: 0,
        b: 0
      }
    }]
  },

  {
    commandName: 'Green',
    commandSequence: [{
      name: 'color',
      value: {
        r: 0,
        g: 255,
        b: 0
      }
    }]
  },

  {
    commandName: 'Blue',
    commandSequence: [{
      name: 'color',
      value: {
        r: 0,
        g: 0,
        b: 255
      }
    }]
  },

  {
    commandName: 'White',
    commandSequence: [{
      name: 'color',
      value: {
        r: 255,
        g: 255,
        b: 255
      }
    }]
  },

  // fourth row
  {
    commandName: 'Orange',
    commandSequence: [{
      name: 'color',
      value: {
        r: 255,
        g: 165,
        b: 0
      }
    }]
  },

  {
    commandName: 'Teal',
    commandSequence: [{
      name: 'color',
      value: {
        r: 0,
        g: 255,
        b: 255
      }
    }]
  },

  {
    commandName: 'Purple',
    commandSequence: [{
      name: 'color',
      value: {
        r: 128,
        g: 0,
        b: 128
      }
    }]
  },

  {
    commandName: 'Pink',
    commandSequence: [{
      name: 'color',
      value: {
        r: 255,
        g: 192,
        b: 203
      }
    }]
  },
];