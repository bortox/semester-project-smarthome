#!/bin/bash

# Funzione ricorsiva per leggere i file nelle cartelle specificate
read_files() {
    local dir="$1"
    local base_dir="$2"

    # Verifica se la directory esiste
    if [ ! -d "$dir" ]; then
        return
    fi

    # Itera su tutti i file e le sottocartelle
    for entry in "$dir"/*; do
        if [ -f "$entry" ]; then
            # Se è un file, stampa il percorso relativo alla base
            echo "$entry"
            cat $entry
        elif [ -d "$entry" ]; then
            # Se è una directory, chiama ricorsivamente la funzione
            read_files "$entry" "$base_dir"
        fi
    done
}

# Cartelle da analizzare
target_dirs=("include" "lib" "src")

# Itera su ogni cartella target
for target in "${target_dirs[@]}"; do
    echo "=== File nella cartella: $target ==="
    if [ -d "$target" ]; then
        read_files "$target" "$(pwd)"
    else
        echo "La cartella '$target' non esiste."
    fi
    echo
done
