#include "affichage.h"

void printBits(char c) {
    // Boucle à travers chaque bit du char
    for (int i = 7; i >= 0; i--) {
        // Masque pour extraire le bit à la position i
        char bit = (c >> i) & 1;
        // Affichage du bit
        printf("%d", bit);
    }
}
