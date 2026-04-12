// collision.h
#ifndef COLLISION_H_
#define COLLISION_H_

#include "cubit_types.h"

/*
Pensa a un AABB non come a un cubo solido, ma come all'intersezione di tre
"fette" (slab). Una fetta è lo spazio compreso tra due piani paralleli.
La fetta X è tutto lo spazio tra il piano a min.x e il piano a max.x.
Stessa cosa per Y e Z. Il box esiste solo dove tutte e tre le fette si
sovrappongono.

Il raggio, viaggiando nello spazio, entra e esce da ciascuna di queste
fette in momenti diversi. Per la fetta X, puoi calcolare il "tempo"
(un valore t lungo il raggio) in cui il raggio entra (t_min_x) e
il tempo in cui esce (t_max_x). Fai lo stesso per Y e Z.

Ora il punto chiave: il raggio è dentro il box solo quando è dentro tutte
e tre le fette contemporaneamente. Quindi prendi il più grande tra i tre
t_min (il momento in cui sei finalmente dentro l'ultima fetta) e il più
piccolo tra i tre t_max (il momento in cui esci dalla prima fetta).
Se il t_min globale è minore del t_max globale, il raggio attraversa il box.
Se no, lo manca.

Facciamo un'analogia per rendere l'idea. Immagina tre porte girevoli una
dietro l'altra su un corridoio. Ognuna si apre e si chiude a intervalli
diversi. Riesci a passare tutte e tre solo se esiste un momento in cui
sono tutte aperte contemporaneamente. Se la prima si chiude prima che la
terza si apra, non passi. Il slab method fa esattamente questo confronto,
ma con le fette spaziali del box.

Una volta che sai che il raggio colpisce, il valore t_min globale ti dà
il punto di impatto. Moltiplichi t per la direzione, lo sommi all'origine,
e hai le coordinate esatte del punto dove il raggio tocca la superficie del box.
La distanza è semplicemente t_min moltiplicato per la lunghezza della direzione
(o direttamente t_min se la direzione è normalizzata).

C'è un dettaglio pratico importante: quando il raggio parte dall'interno del
box, t_min risulta negativo. Questo succede per esempio se fai un raycast
dal giocatore verso il basso per controllare il terreno, e il punto di
partenza è dentro il collider del giocatore stesso. Devi gestire questo
caso — tipicamente ignorando l'oggetto di origine, oppure scartando
i risultati con t negativo.

Ultimo pezzo: quando fai un raycast nella scena, non ti interessa qualsiasi
oggetto colpito, ti interessa il più vicino. Quindi scorri tutti i collider,
testi il raggio contro ognuno, e tieni traccia di quello con il t_min più basso.
Alla fine restituisci un risultato che contiene l'oggetto colpito, il punto di
impatto e la distanza. Se nessuno viene colpito, restituisci un risultato
"vuoto" — magari con un puntatore nullo.
*/

void collision_init(void);
void collision_shutdown(void);

#endif // COLLISION_H_
