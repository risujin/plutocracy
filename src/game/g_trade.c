/******************************************************************************\
 Plutocracy - Copyright (C) 2008 - Michael Levin

 This program is free software; you can redistribute it and/or modify it under
 the terms of the GNU General Public License as published by the Free Software
 Foundation; either version 2, or (at your option) any later version.

 This program is distributed in the hope that it will be useful, but WITHOUT
 ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.
\******************************************************************************/

/* Functions for trading */

#include "g_common.h"

/******************************************************************************\
 Returns FALSE if the two cargo structures differ in a significant way.
\******************************************************************************/
bool G_cargo_equal(const g_cargo_t *a, const g_cargo_t *b)
{
        if (a->auto_buy != b->auto_buy || a->auto_sell != b->auto_sell)
                return FALSE;
        if (a->auto_buy &&
            (a->maximum != b->maximum || a->buy_price != b->buy_price))
                return FALSE;
        if (a->auto_sell &&
            (a->minimum != b->minimum || a->sell_price != b->sell_price))
                return FALSE;
        return TRUE;
}

/******************************************************************************\
 Returns the space one unit of a given cargo takes up.
\******************************************************************************/
static float cargo_space(g_cargo_type_t cargo)
{
        if (cargo == G_CT_GOLD)
                return 0.01f;
        return 1.f;
}

/******************************************************************************\
 Returns a single-character suffix a cargo item.
\******************************************************************************/
static char cargo_suffix(g_cargo_type_t cargo)
{
        return tolower(g_cargo_names[cargo][0]);
}

/******************************************************************************\
 Returns the amount of space the given cargo manifest contains. Updates the
 store's cached space count.
\******************************************************************************/
int G_store_space(g_store_t *store)
{
        int i;

        for (store->space_used = i = 0; i < G_CARGO_TYPES; i++) {
                if (store->cargo[i].amount < 0)
                        continue;
                store->space_used += (int)ceilf(cargo_space(i) *
                                                store->cargo[i].amount);
        }
        return store->space_used;
}

/******************************************************************************\
 Add or subtract cargo from a store. Returns the amount actually added or
 subtracted.
\******************************************************************************/
int G_store_add(g_store_t *store, g_cargo_type_t cargo, int amount)
{
        int excess;

        if(amount == 0)
                return 0;

        /* Store is already overflowing */
        if (store->space_used > store->capacity)
                return 0;
        store->modified |= 1 << cargo;

        /* Don't take more than what's there */
        if (amount < -store->cargo[cargo].amount)
                amount = -store->cargo[cargo].amount;

        /* Don't put in more than it can hold */
        store->cargo[cargo].amount += amount;
        if ((excess = G_store_space(store) - store->capacity) > 0) {
                store->cargo[cargo].amount -= (int)(excess /
                                                    cargo_space(cargo));
                store->space_used = store->capacity;
        }
        C_assert(store->cargo[cargo].amount >= 0);

        return amount;
}

/******************************************************************************\
 Add the values of [cost] to the store.
\******************************************************************************/
void G_store_add_cost(g_store_t *store, const g_cost_t *cost)
{
        int i;

        for (i = 0; i < G_CARGO_TYPES; i++)
                G_store_add(store, i, cost->cargo[i]);
}

/******************************************************************************\
 Returns the amount of a cargo that a store can transfer from another store.
\******************************************************************************/
int G_limit_purchase(g_store_t *buyer, g_store_t *seller,
                     g_cargo_type_t cargo, int amount, bool free)
{
        int limit, price;
        bool reversed;

        price = (free) ? 0 : seller->cargo[cargo].sell_price;

        /* Make sure we have updated store space */
        G_store_space(buyer);
        G_store_space(seller);

        /* Negative amount is a sale */
        if ((reversed = amount < 0)) {
                g_store_t *temp;

                price = (free) ? 0 : seller->cargo[cargo].buy_price;
                temp = buyer;
                buyer = seller;
                seller = temp;
                amount = -amount;

                /* Buying too much */
                limit = buyer->cargo[cargo].maximum -
                        buyer->cargo[cargo].amount;
                if (amount > limit)
                        amount = limit;
        }

        /* Purchase */
        else {
                /* Selling too much */
                limit = seller->cargo[cargo].amount -
                        seller->cargo[cargo].minimum;
                if (amount > limit)
                        amount = limit;
        }

        /* How much does the seller have? */
        if (amount > (limit = seller->cargo[cargo].amount -
                              seller->cargo[cargo].minimum))
                amount = limit;

        /* How much can buyer afford? */
        if (price > 0 &&
            amount > (limit = buyer->cargo[G_CT_GOLD].amount / price))
                amount = limit;

        /* How much can buyer fit (accounting for price)? */
        limit = buyer->capacity - buyer->space_used +
                (cargo_space(cargo) - cargo_space(G_CT_GOLD)) * amount * price;
        if (amount > limit)
                amount = limit;

        /* How much can seller fit (accounting for price)? */
        limit = seller->capacity - seller->space_used +
                (cargo_space(G_CT_GOLD) - cargo_space(cargo)) * amount * price;
        if (amount > limit)
                amount = limit;

        if (amount < 0)
                return 0;
        return reversed ? -amount : amount;
}

/******************************************************************************\
 Select the clients that can see this store.
\******************************************************************************/
void G_store_select_clients(const g_store_t *store)
{
        int i;

        if (!store)
                return;
        for (i = 0; i < N_CLIENTS_MAX; i++)
                n_clients[i].selected = store->visible[i];
}

/******************************************************************************\
 Initialize the store structure, set default prices, minimums, and maximums.
\******************************************************************************/
g_store_t *G_store_init(int capacity)
{
        g_store_t *store;
        int i;

        store = (g_store_t*)Store_new(&StoreType, NULL, NULL);
        store->capacity = capacity;
        for (i = 0; i < G_CARGO_TYPES; i++) {
                store->cargo[i].maximum = (int)(capacity / cargo_space(i));
                store->cargo[i].buy_price = 50;
                store->cargo[i].sell_price = 50;
        }

        /* Buying/selling gold refers to donations so do not charge for it */
        store->cargo[G_CT_GOLD].buy_price = 0;
        store->cargo[G_CT_GOLD].sell_price = 0;
        store->cargo[G_CT_GOLD].auto_buy = TRUE;

        /* Don't want to sell all of your crew */
        store->cargo[G_CT_CREW].minimum = 1;
        return store;
}
/******************************************************************************\
 Convert a python list or tuple to a g_cost_t
\******************************************************************************/
void G_list_to_cost( PyObject *list, g_cost_t *cost )
{
        C_zero(cost);
        int i;
        PyObject *fast;

        fast = PySequence_Fast( list, "Class cost must be a sequence" );

        if (!fast) return;
        if (PySequence_Fast_GET_SIZE(fast) != G_CARGO_TYPES)
        {
                Py_DECREF(fast);
                return;
        }

        for (i=0; i<G_CARGO_TYPES; i++)
        {
                cost->cargo[i] = PyInt_AsLong(PySequence_Fast_GET_ITEM(fast, i));
        }
        Py_DECREF(fast);
}

/******************************************************************************\
 Determines whether a player can make a payment at a specific tile, returns
 TRUE if payment is possible.
\******************************************************************************/
bool G_pay(n_client_id_t client, int tile, const g_cost_t *cost, bool pay)
{
        g_cost_t unpaid;
        int i, j, neighbors[3];
        /* Record if we have found something to make the payment */
        bool foundpayer;

        if (!cost)
                return FALSE;
        unpaid = *cost;
        foundpayer = FALSE;

        /* Just search neighboring ships and buildings until plots are
         * implemented */
        R_tile_neighbors(tile, neighbors);
        for (i = -1; i < 3; i++) {
                g_store_t *store;
                g_ship_t *ship;
                bool modified;

                if(i>=0) {
                        ship = g_tiles[neighbors[i]].ship;
                    if (!G_ship_controlled_by(ship, client) ||
                        ship->rear_tile >= 0)
                            continue;
                    store = ship->store;
                    foundpayer = TRUE;
                } else if (g_tiles[tile].building) {
                        store = g_tiles[tile].building->store;
                        foundpayer = TRUE;
                }
                else
                        continue;

                /* Check what this ship can pay */
                for (modified = FALSE, j = 0; j < G_CARGO_TYPES; j++) {
                        int available;

                        if ((available = store->cargo[j].amount) <= 0)
                                continue;
                        if (available > unpaid.cargo[j])
                                available = unpaid.cargo[j];
                        unpaid.cargo[j] -= available;

                        /* Actually do the transfer */
                        if (pay) {
                                G_store_add(store, j, -available);
                                modified = TRUE;
                        }
                }
        }
        /* Can't pay if there isn't anything to make the payment */
        if(!foundpayer)
                return FALSE;
        /* See if everything has been paid for */
        for (i = 0; i < G_CARGO_TYPES; i++)
                if (unpaid.cargo[i] > 0)
                        return FALSE;
        return TRUE;
}

/******************************************************************************\
 Add the cargo contents to the current network message. Note that this call
 does not actually send the information, you must finish and send the message
 yourself with N_send(NULL).

 If [force] is TRUE, modified status is ignored and unchanged after this call.
\******************************************************************************/
void G_store_send(g_store_t *store, bool force)
{
        int i;

        C_assert(N_CLIENTS_MAX <= 32);
        N_send_int(force ? -1 : store->modified);
        for (i = 0; i < G_CARGO_TYPES; i++) {
                g_cargo_t *cargo;

                if (!force && !(store->modified & (1 << i)))
                        continue;
                cargo = store->cargo + i;
                N_send_short(cargo->amount);
                N_send_short(cargo->auto_buy ? cargo->buy_price : -1);
                N_send_short(cargo->auto_sell ? cargo->sell_price : -1);
                N_send_short(cargo->minimum);
                N_send_short(cargo->maximum);
        }
        if (!force)
                store->modified = 0;
}

/******************************************************************************\
 Read the cargo contents to the message receive buffer. If [ignore_prices] is
 TRUE, price settings will not be overwritten.
\******************************************************************************/
void G_store_receive(g_store_t *store, bool ignore_prices)
{
        int i, modified;

        C_assert(N_CLIENTS_MAX <= 32);
        modified = N_receive_int();
        if (!modified)
                return;
        for (i = 0; i < G_CARGO_TYPES; i++) {
                g_cargo_t *cargo;

                if (!(modified & (1 << i)))
                        continue;
                cargo = store->cargo + i;
                cargo->amount = N_receive_short();
                if (ignore_prices) {
                        N_receive_short();
                        N_receive_short();
                        N_receive_short();
                        N_receive_short();
                        continue;
                }
                cargo->buy_price = N_receive_short();
                cargo->sell_price = N_receive_short();
                cargo->auto_buy = cargo->buy_price >= 0;
                cargo->auto_sell = cargo->sell_price >= 0;
                cargo->minimum = N_receive_short();
                cargo->maximum = N_receive_short();
        }
        G_store_space(store);
}

/******************************************************************************\
 Generate a string description of a cost structure.
\******************************************************************************/
const char *G_cost_to_string(const g_cost_t *cost)
{
        static char buf[128];
        char *pos, *fmt;
        int i, space;
        bool first;

        pos = buf;
        first = TRUE;
        for (i = 0; i < G_CARGO_TYPES; i++) {
                if (cost->cargo[i] < 1)
                        continue;
                if (first) {
                        fmt = "%d%c";
                        first = FALSE;
                } else
                        fmt = ", %d%c";
                space = (int)(buf + sizeof (buf) - pos);
                if (space < 1)
                        return buf;
                pos += snprintf(pos, space, fmt,
                                cost->cargo[i], cargo_suffix(i));
        }
        return buf;
}

