module.exports = [
  {
    "type": "heading",
    "defaultValue": "Watchface Settings"
  },
  {
    "type": "text",
    "defaultValue": "Customize the clock's appearance and preferences."
  },
    {
    "type": "section",
    "items": [
      {
        "type": "heading",
        "defaultValue": "Preferences"
      },
        {
        "type": "toggle",
        "messageKey": "TEMPCELSIUS",
        "label": "Use Celsius",
        "defaultValue": false
      },

    ]
    },
  {
    "type": "section",
    "items": [
      {
        "type": "heading",
        "defaultValue": "Visibility"
      },
      {
        "type": "toggle",
        "messageKey": "SHOWCLOCK",
        "label": "Show Clock",
        "defaultValue": true
      },
      {
        "type": "toggle",
        "messageKey": "SHOWDATE",
        "label": "Show Date",
        "defaultValue": true
      },
      {
        "type": "toggle",
        "messageKey": "SHOWSTEPS",
        "label": "Show Steps",
        "defaultValue": true
      },
      {
        "type": "toggle",
        "messageKey": "SHOWWEATHER",
        "label": "Show Weather",
        "defaultValue": true
      },
      {
        "type": "toggle",
        "messageKey": "SHOWBATTERY",
        "label": "Show Battery",
        "defaultValue": true
      },

    ]
  },
  {
    "type": "submit",
    "defaultValue": "Save Settings"
  }
];
