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
        "messageKey": "CELSIUS",
        "label": "Use Celsius",
        "defaultValue": false
      },
      {
        "type": "toggle",
        "messageKey": "DYNAMICIMAGE",
        "label": "Weather-Based Backdrop",
        "defaultValue": false
      },
        {
        "type": "select",
        "messageKey": "FORCESEASON",
        "defaultValue": -1,
        "label": "Static Backdrop Weather",
        "options": [
            { 
            "label": "Disabled", 
            "value": -1
            },
            { 
            "label": "Clear", 
            "value": 0 
            },
            { 
            "label": "Partly Cloudy",
            "value": 1 
            },
            { 
            "label": "Cloudy",
            "value": 2
            },
            { 
            "label": "Rain",
            "value": 3
            },
            {
                "label":"Lightning",
                "value": 4
            },
            {
                "label":"Snow",
                "value": 5
            }
        ]
        }
    ]
    },
  {
    "type": "submit",
    "defaultValue": "Save Settings"
  }
];
