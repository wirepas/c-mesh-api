#!/usr/bin/env python3

import sys
import time

import wirepas.mesh_api as wm

c = wm.Connection(b"/dev/ttyACM0", 125000)

try:
    addr = "%d" % c.get_node_address()
except wm.MeshAPIError:
    addr = "<not set>"

try:
    nw_addr = c.get_network_address()
    nw_addr = "%d (0x%06x)" % (nw_addr, nw_addr)
except wm.MeshAPIError:
    nw_addr = "<not set>"

try:
    nw_channel = "%d" % c.get_network_channel()
except wm.MeshAPIError:
    nw_channel = "<not set>"

try:
    role = "%s" % c.get_role()
except wm.MeshAPIError:
    role = "<not set>"

auth_key_set = c.is_authentication_key_set() and "<set>" or "<not set>"
cipher_key_set = c.is_cipher_key_set() and "<set>" or "<not set>"

try:
    diag_interval = c.get_app_config_data().interval
except wm.MeshAPIError:
    diag_interval = "<not set>"

stack_status = c.get_stack_status()

print("node address:        %s" % addr)
print("network address:     %s" % nw_addr)
print("network channel:     %s" % nw_channel)
print("role:                %s" % role)
print("authentication key:  %s" % auth_key_set)
print("encryption key:      %s" % cipher_key_set)
print("diagnostic interval: %s" % diag_interval)
print("stack status:        %s" % stack_status)


def app_config_data_callback(app_config_data):
    sys.stdout.write(
        "\nIn app_config_data_callback(app_config_data = %s)\n" % repr(app_config_data)
    )
    sys.stdout.flush()


c.register_for_app_config_data(app_config_data_callback)


def reception_callback(data):
    sys.stdout.write("\nIn reception_callback(data = %s)\n" % repr(data))
    sys.stdout.flush()


for n in range(256):
    c.register_for_data(n, reception_callback)


def scan_callback(scan_ready):
    sys.stdout.write("\nIn scan_callback(scan_ready = %s)\n" % repr(scan_ready))
    sys.stdout.flush()


c.register_for_scan_neighbors_done(scan_callback)


def other_callback(*args, **kwargs):
    sys.stdout.write("\nIn other_callback(%s, %s)\n" % (args, kwargs))
    sys.stdout.flush()


c.register_for_remote_status(other_callback)
c.register_from_stack_status(other_callback)


def wait():
    while True:
        time.sleep(1)
        sys.stdout.write(".")
        sys.stdout.flush()


wait()
