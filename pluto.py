#!/usr/bin/env python

###############################################################################
# Plutocracy - Copyright (C) 2008 - John Black                                #
#                                                                             #
# This program is free software; you can redistribute it and/or modify it     #
# under the terms of the GNU General Public License as published by the Free  #
# Software Foundation; either version 2, or (at your option) any later        #
# version.                                                                    #
#                                                                             #
# This program is distributed in the hope that it will be useful, but WITHOUT #
# ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or       #
# FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for    #
# more details.                                                               #
###############################################################################

import sys, urllib, csv, threading, os
from distutils.util import get_platform
from os.path import *

sys.path.append("build/lib.%s-%s" % (get_platform(), sys.version[0:3]))

linked_files = []

def copy_font(font):
    if not isdir(join(os.environ["HOME"], ".fonts")):
        os.mkdir(join(os.environ["HOME"], ".fonts"))

    newpath = join(os.environ["HOME"], ".fonts", font)
    if not exists(newpath):
        os.symlink(join(os.getcwd(), "gui", "fonts", font), newpath)
        linked_files.append(newpath)
        
def delete_fonts():
    for file in linked_files:
        os.remove(file)
        
copy_font("BLKCHCRY.TTF")
copy_font("LCD2U___.TTF")
copy_font("SF_Archery_Black.ttf")

import plutocracy

# TODO: move these into a python plutocracy.constants module
G_CT_GOLD = plutocracy.game.G_CT_GOLD
G_CT_CREW = plutocracy.game.G_CT_CREW
G_CT_RATIONS = plutocracy.game.G_CT_RATIONS
G_CT_WOOD = plutocracy.game.G_CT_WOOD
G_CT_IRON = plutocracy.game.G_CT_IRON

def empty_tile_click(emtpy, client_id, tile):
    print "empty_tile_click_called"
    buildingclasses = plutocracy.game.get_buildingclasses()
    plutocracy.interface.reset_ring()
    for buildingclass in buildingclasses:
        if not buildingclass.buildable: continue
        can_pay = plutocracy.game.pay(client_id, tile,
                                      buildingclass.cost, False)
        cost_string = plutocracy.game.cost_to_string(buildingclass.cost)
        plutocracy.interface.add_to_ring(buildingclass.ring_id, can_pay,
                                         buildingclass.name, cost_string)
    plutocracy.interface.show_ring(plutocracy.game.tile_ring_callback)
    return True

def shipyard_tile_click(shipyard, client_id, tile, *args):
    print "shipyard_click called"
    shipclasses = plutocracy.game.get_shipclasses()
    plutocracy.interface.reset_ring()
    for shipclass in shipclasses:
        can_pay = plutocracy.game.pay(client_id, tile, shipclass.cost, False)
        cost_string = plutocracy.game.cost_to_string(shipclass.cost)
        plutocracy.interface.add_to_ring(shipclass.ring_id, can_pay,
                                         shipclass.name, cost_string)
    plutocracy.interface.show_ring(plutocracy.game.tile_ring_callback)
    return True

def shipyard_tile_ring_command(shipyard, client_id, tile, icon):
    print "shipyard_tile_ring_command called"
    shipclass = plutocracy.game.ship_class_from_ring_id(icon)
    if not plutocracy.game.pay(client_id, tile, shipclass.cost, False):
        return
    ship = plutocracy.game.ship_spawn(-1, client_id, tile, shipclass)
    if not ship:
        print "ship invalid"
        return
    ship.store.add(G_CT_CREW, 10)
    ship.store.add(G_CT_RATIONS, 10)
#    ship.store.add(G_CT_GOLD, 1000)
#    ship.store.add(G_CT_WOOD, 75)
#    ship.store.add(G_CT_IRON, 25)
    plutocracy.game.pay(client_id, tile, shipclass.cost, True)
    return

class ServerListRefresh(threading.Thread):
    def __init__(self, master, url):
        self.master = master
        self.url = url
        threading.Thread.__init__(self)
    def run(self):
        global csv_file
        print "fetching server list"
        csv_file = urllib.urlopen("http://" + self.master + self.url +
                                  "?format=csv")

def parse_servers():
    global csv_file
    server_list = csv.DictReader(csv_file)
    csv_file = None
    print "parsing server list"
    plutocracy.interface.reset_servers()
    for server in server_list:
        protocol = int(server["protocol"])
        name = server["name"]
        address = server["address"]
        info = server["info"]
        compatible = protocol == plutocracy.game.G_PROTOCOL
        print "Parsed server '%s (%s) %s'" % (address, name, info)
        plutocracy.interface.add_server(name, info, address, compatible)
        
def refresh_servers(master, url):
    thread = ServerListRefresh(master, url)
    thread.start()

empty = plutocracy.game.BuildingClass("Empty", "", "")
empty.connect("tile-click", empty_tile_click)

tree = plutocracy.game.BuildingClass("Trees", "models/tree/deciduous.plum",
                                     "gui/icons/ring/tree.png")
tree.health = 100

shipyard = plutocracy.game.BuildingClass("Shipyard", "models/water/dock.plum",
                                         "gui/icons/ring/dock.png")
shipyard.buildable = True
shipyard.health = 250
shipyard.cargo = 750
shipyard.cost[G_CT_GOLD] = 0 #800
shipyard.cost[G_CT_WOOD] = 0 #50
shipyard.cost[G_CT_IRON] = 0 #50
shipyard.connect("tile-click", shipyard_tile_click)
shipyard.connect("tile-ring-command", shipyard_tile_ring_command)

plutocracy.game.add_buildingclass(empty)
plutocracy.game.add_buildingclass(tree)
plutocracy.game.add_buildingclass(shipyard)

sloop = plutocracy.game.ShipClass("Sloop", "models/ship/sloop.plum",
                                  "gui/icons/ring/ship.png")
sloop.speed = 1.5
sloop.max_health = 50
sloop.cargo = 100
sloop.cost[G_CT_GOLD] = 800
sloop.cost[G_CT_WOOD] = 50
sloop.cost[G_CT_IRON] = 50

supersloop = plutocracy.game.ShipClass("Super Sloop", "models/ship/sloop.plum",
                                       "gui/icons/ring/ship+.png")
supersloop.speed = 3.0
supersloop.max_health = 100
supersloop.cargo = 200
supersloop.cost[G_CT_GOLD] = 0# 1600
supersloop.cost[G_CT_WOOD] = 0 #100
supersloop.cost[G_CT_IRON] = 0 #100

spider = plutocracy.game.ShipClass("Spider", "models/ship/spider.plum",
                                   "gui/icons/ring/spider.png")
spider.speed = 1.25
spider.max_health = 75
spider.cargo = 250
spider.cost[G_CT_GOLD] = 1000
spider.cost[G_CT_WOOD] = 75
spider.cost[G_CT_IRON] = 25

galleon = plutocracy.game.ShipClass("Galleon", "models/ship/galleon.plum",
                                    "gui/icons/ring/galleon.png")
galleon.speed = 1.0
galleon.max_health = 100
galleon.cargo = 400
galleon.cost[G_CT_GOLD] = 1600
galleon.cost[G_CT_WOOD] = 125
galleon.cost[G_CT_IRON] = 75

plutocracy.game.add_shipclass(sloop)
plutocracy.game.add_shipclass(supersloop)
plutocracy.game.add_shipclass(spider)
plutocracy.game.add_shipclass(galleon)

csv_file = None

plutocracy.init()
plutocracy.game.connect("refresh-servers", refresh_servers)
try:
    while 1:
        plutocracy.c_update() # C update function
        #plutocracy.update() # Python update function
        if plutocracy.check_exit(): break
        if csv_file: parse_servers()
    plutocracy.common.write_autogen()
finally:
    plutocracy.cleanup()
    delete_fonts()
