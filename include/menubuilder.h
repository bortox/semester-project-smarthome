#pragma once
#include "LcdMenuLib.h"
#include "HomeManager.h"

namespace MenuBuilder {

    // Funzione che crea il sottomenù specifico per UN dispositivo
    MenuScreen* createSubMenuForDevice(Device* device) {
        // Alloca dinamicamente una nuova schermata per questo dispositivo
        MenuScreen* screen = new MenuScreen(device->getName());

        switch (device->getDeviceType()) {
            
            case DeviceType::SIMPLE_LIGHT: {
                auto* light = static_cast<SimpleLight*>(device); // Cast sicuro
                screen->addItem(new ItemBasic("Toggle", [light](){ light->toggle(); }));
                screen->addItem(new ItemBasic("Flash", [light](){ light->flash(500); }));
                break;
            }

            case DeviceType::DIMMABLE_LIGHT: {
                auto* light = static_cast<DimmableLight*>(device);
                screen->addItem(new ItemBasic("Toggle", [light](){ light->toggle(); }));
                // Aggiungiamo un range per la luminosità
                screen->addItem(new ItemRange<int>("Brightness", light->getBrightness(), 10, 0, 100, 
                    [light](int val){ light->setBrightness(val); }, "%d%%"));
                break;
            }

            case DeviceType::RGB_LIGHT: {
                // Logica più complessa con sottomenù per colori, ecc.
                break;
            }

            case DeviceType::TEMP_SENSOR: {
                auto* sensor = static_cast<TemperatureSensor*>(device);
                // NOTA: ITEM_BASIC ha testo statico. Per un valore dinamico,
                // la soluzione più semplice è un item che, se cliccato,
                // aggiorna e stampa su seriale, o una custom class.
                // Per ora, lo facciamo non cliccabile o con un'azione di refresh.
                screen->addItem(new ItemBasic( (sensor->getValueString() + sensor->getUnitString()).c_str() ));
                break;
            }
            // ... altri casi ...
        }
        
        screen->addItem(new ItemBack()); // Aggiunge sempre l'opzione "Indietro"
        return screen;
    }

    // Funzione che popola una schermata di zona (es. "Dentro")
    void populateZoneScreen(MenuScreen& zoneScreen, const std::vector<Device*>& devices) {
        for (Device* device : devices) {
            // Per ogni dispositivo, crea il suo sottomenù personale
            MenuScreen* deviceSubMenu = createSubMenuForDevice(device);
            // Aggiunge un item alla schermata della zona che punta a quel sottomenù
            zoneScreen.addItem(new ItemSubMenu(device->getName(), *deviceSubMenu));
        }
    }
}