module.exports = [
    {
        "type": "heading",
        "defaultValue": "The World Machine Settings"
    },
    {
        "type": "section",
        "items": [
            {
                "type": "heading",
                "defaultValue": "Customize"
            },
            {
              "type": "select",
                  "messageKey": "ClockMode",
                  "label": "Time Format",
                  "defaultValue": "0",
                  "options": [
                    { "label": "System", "value": "0" },
                    { "label": "12-Hour", "value": "1" },
                    { "label": "24-Hour", "value": "2" }
                ]
            },
            {
                "type": "toggle",
                "messageKey": "LeadingZeros",
                "label": "Show Leading Zeros (08:00)",
                "defaultValue": false
            },
            {
                "type": "toggle",
                "messageKey": "ShowAMPM",
                "label": "Show AM/PM",
                "defaultValue": true
            },
        ]
    },
    {
        "type": "section",
        "items": [
            {
                "type": "heading",
                "defaultValue": "Wallpapers"
            },
            {
              "type": "select",
                  "messageKey": "Wallpaper",
                  "label": "Wallpaper",
                  "defaultValue": "1",
                  "options": [
                    { "label": "The World Machine", "value": "1" },
                    { "label": "Barrens Craters", "value": "2" },
                    { "label": "Glen Shoreline", "value": "3" },
                    { "label": "Refuge Cityscape", "value": "4" },
                    { "label": "Messiah", "value": "5" },
                    { "label": "My Burden is Light", "value": "6" },
                    { "label": "Asteroid", "value": "7" },
                    { "label": "Blank", "value": "0" }
                ]
            }
        ]
    },
    {
        "type": "section",
        "capabilities": ["BW"],
        "items": [
            {
                "type": "heading",
                "defaultValue": "Themes"
            },
            {
              "type": "toggle",
              "messageKey": "Inverted",
              "label": "Inverted UI",
              "defaultValue": false
            }
        ]
    },
    {
        "type": "section",
        "capabilities": ["COLOR"],
        "items": [
            {
                "type": "heading",
                "defaultValue": "Themes"
            },
            {
              "type": "color",
              "messageKey": "UI_Color",
              "defaultValue": "aa55ff",
              "label": "PLACEHOLDER",
              "sunlight": false
            }
        ]
    },
    {
        "type": "submit",
        "defaultValue": "Save Settings"
    }
];