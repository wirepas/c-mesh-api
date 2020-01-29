# _mesh_api.pyx - Python wrapper for Wirepas C Mesh API library

# cython: language_level=3

# NOTE: Some naming has been changed from the C Mesh API, for consistency:
#
#  - src, source -> source
#  - dest, dst, destination -> dest
#  - add, addr, address -> address
#  - ch, chan, channel -> channel
#  - nw, net, network -> network
#  - scrat, scratchpad -> scratchpad
#  - seq, seq_number, sequence -> seq
#  - ep, endpoint -> endpoint
#  - ll, low-latency -> low-latency
#  - autorole, auto-role: autorole
#  - nbor, neighbor: neighbor

# The following features are missing from the C-Mesh-API library:
#
# TODO: Missing CSAP attributes: cOfflineScan, cFeatureLockBits, cFeatureLockKey
# TODO: Missing MSAP attributes: mMulticastGroups
# TODO: MSAP-NON-ROUTER LONG SLEEP (NRLS) Service
# TODO: TSAP Services


#### Imports

from libc.string cimport memset
from libc.stdint cimport uint8_t, uint16_t, uint32_t
from libcpp cimport bool

cimport wpc


#### Types

ctypedef wpc.app_addr_t addr_t

ctypedef wpc.net_addr_t network_addr_t

ctypedef wpc.net_channel_t network_channel_t


#### Constants

# Role bits
ROLE_SINK                       = wpc.APP_ROLE_SINK
ROLE_HEADNODE                   = wpc.APP_ROLE_HEADNODE
ROLE_SUBNODE                    = wpc.APP_ROLE_SUBNODE
ROLE_UNKNOWN                    = wpc.APP_ROLE_UNKNOWN
ROLE_OPTION_LOW_LATENCY         = wpc.APP_ROLE_OPTION_LL
ROLE_OPTION_RELAY               = wpc.APP_ROLE_OPTION_RELAY
ROLE_OPTION_AUTOROLE            = wpc.APP_ROLE_OPTION_AUTOROLE

# Quality-of-service values
QOS_NORMAL                      = wpc.APP_QOS_NORMAL
QOS_HIGH                        = wpc.APP_QOS_HIGH

# Special address values, with explicit conversion to unsigned type
ADDRESS_ANYSINK                 = <wpc.app_addr_t> wpc.APP_ADDR_ANYSINK
ADDRESS_BROADCAST               = <wpc.app_addr_t> wpc.APP_ADDR_BROADCAST

# Stack state bits
STATE_STACK_STOPPED             = wpc.APP_STACK_STOPPED
STATE_NETWORK_ADDRESS_NOT_SET   = wpc.APP_STACK_NETWORK_ADDRESS_NOT_SET
STATE_NODE_ADDRESS_NOT_SET      = wpc.APP_STACK_NODE_ADDRESS_NOT_SET
STATE_NETWORK_CHANNEL_NOT_SET   = wpc.APP_STACK_NETWORK_CHANNEL_NOT_SET
STATE_ROLE_NOT_SET              = wpc.APP_STACK_ROLE_NOT_SET

# Return codes from C Mesh API functions
RES_OK                          = wpc.APP_RES_OK
RES_STACK_NOT_STOPPED           = wpc.APP_RES_STACK_NOT_STOPPED
RES_STACK_ALREADY_STOPPED       = wpc.APP_RES_STACK_ALREADY_STOPPED
RES_STACK_ALREADY_STARTED       = wpc.APP_RES_STACK_ALREADY_STARTED
RES_INVALID_VALUE               = wpc.APP_RES_INVALID_VALUE
RES_ROLE_NOT_SET                = wpc.APP_RES_ROLE_NOT_SET
RES_NODE_ADDRESS_NOT_SET        = wpc.APP_RES_NODE_ADD_NOT_SET
RES_NETWORK_ADDRESS_NOT_SET     = wpc.APP_RES_NET_ADD_NOT_SET
RES_NETWORK_CHANNEL_NOT_SET     = wpc.APP_RES_NET_CHAN_NOT_SET
RES_STACK_IS_STOPPED            = wpc.APP_RES_STACK_IS_STOPPED
RES_NODE_NOT_A_SINK             = wpc.APP_RES_NODE_NOT_A_SINK
RES_UNKNOWN_DEST                = wpc.APP_RES_UNKNOWN_DEST
RES_NO_CONFIG                   = wpc.APP_RES_NO_CONFIG
RES_ALREADY_REGISTERED          = wpc.APP_RES_ALREADY_REGISTERED
RES_NOT_REGISTERED              = wpc.APP_RES_NOT_REGISTERED
RES_ATTRIBUTE_NOT_SET           = wpc.APP_RES_ATTRIBUTE_NOT_SET
RES_ACCESS_DENIED               = wpc.APP_RES_ACCESS_DENIED
RES_DATA_ERROR                  = wpc.APP_RES_DATA_ERROR
RES_NO_SCRATCHPAD_START         = wpc.APP_RES_NO_SCRATCHPAD_START
RES_NO_VALID_SCRATCHPAD         = wpc.APP_RES_NO_VALID_SCRATCHPAD
RES_NOT_A_SINK                  = wpc.APP_RES_NOT_A_SINK
RES_OUT_OF_MEMORY               = wpc.APP_RES_OUT_OF_MEMORY
RES_INVALID_DIAG_INTERVAL       = wpc.APP_RES_INVALID_DIAG_INTERVAL
RES_INVALID_SEQ                 = wpc.APP_RES_INVALID_SEQ
RES_INVALID_START_ADDRESS       = wpc.APP_RES_INVALID_START_ADDRESS
RES_INVALID_NUMBER_OF_BYTES     = wpc.APP_RES_INVALID_NUMBER_OF_BYTES
RES_INVALID_SCRATCHPAD          = wpc.APP_RES_INVALID_SCRATCHPAD
RES_INVALID_REBOOT_DELAY        = wpc.APP_RES_INVALID_REBOOT_DELAY
RES_INTERNAL_ERROR              = wpc.APP_RES_INTERNAL_ERROR

# Miscellaneous constants
APP_CONFIG_DATA_MAX_NUM_BYTES   = wpc.APP_CONFIG_DATA_MAX_NUM_BYTES
CIPHER_KEY_NUM_BYTES            = wpc.CIPHER_KEY_NUM_BYTES
AUTHENTICATION_KEY_NUM_BYTES    = wpc.AUTHENTICATION_KEY_NUM_BYTES


#### Global variables

# The C Mesh API library only supports one connection at a time
cdef bool _in_use = False

cdef uint8_t _app_config_data_num_bytes = 0

cdef object _app_config_data_reception_cb = None

cdef object _data_reception_cbs = {}

cdef object _remote_status_reception_cb = None

cdef object _neighbor_scan_done_cb = None

cdef object _stack_status_cb = None


#### Functions

cdef bool _open_connection(const unsigned char[:] port_name,
                           unsigned long bitrate) except False:
    '''Open C Mesh API connection'''

    global _in_use

    if _in_use:
        # C Mesh API connection already in use
        raise MeshAPIError(wpc.APP_RES_INTERNAL_ERROR,
                           "resource already in use")

    # Port name is expected to be pure ASCII, which
    # it is on Linux, Windows and Mac
    cdef wpc.app_res_e res = wpc.WPC_initialize(<const char *> &port_name[0],
                                                bitrate)
    if res != wpc.APP_RES_OK:
        # Custom text for exception, as the C Mesh API library only
        # returns APP_RES_INTERNAL_ERROR, which is not very descriptive
        raise MeshAPIError(res, "could not open connection")

    _in_use = True

    return True


cdef void _close_connection():
    '''Close C Mesh API connection'''

    global _in_use
    if _in_use:
        wpc.WPC_close()
        _in_use = False


cdef bool _res_to_exc(wpc.app_res_e res) except False:
    '''Convert app_res_e to a Python exception'''

    if res != wpc.APP_RES_OK:
        # Map app_res_e to an exception
        raise MeshAPIError(res)
    return True


cdef void _app_config_data_reception_cb_c(uint8_t seq,
                                          uint16_t interval,
                                          const uint8_t* config_p) with gil:
    '''C callback for app config data reception'''

    global _app_config_data_reception_cb, _app_config_data_num_bytes
    cdef object cb

    cb = _app_config_data_reception_cb
    if cb:
        app_config_data = AppConfigData(
            seq = seq,
            interval = interval,
            config_bytes = config_p[:_app_config_data_num_bytes])
        try:
            # Call the registered Python callback
            cb(app_config_data)
        except:
            # Ignore exceptions from callbacks, TODO: Don't ignore, report
            pass


cdef bool _data_reception_cb_c(const uint8_t* bytes_,
                               uint8_t num_bytes,
                               wpc.app_addr_t source_address,
                               wpc.app_addr_t dest_address,
                               wpc.app_qos_e qos,
                               uint8_t source_endpoint,
                               uint8_t dest_endpoint,
                               uint32_t travel_time_ms,
                               uint8_t hop_count,
                               unsigned long long timestamp_ms_epoch) with gil:
    '''C callback for data reception'''

    global _data_reception_cbs
    cdef object data
    cdef object cb

    cb = _data_reception_cbs.get(dest_endpoint, None)
    if cb:
        try:
            # Call the registered Python callback
            data = ReceivedData(
                bytes_ = bytes_[:num_bytes], # NOTE: Memoryview not valid
                                             #       after the callback returns
                source_address = source_address,
                dest_address = dest_address,
                qos = qos,
                source_endpoint = source_endpoint,
                dest_endpoint = dest_endpoint,
                travel_time_s = travel_time_ms / 1000.0, # Convert to seconds
                hop_count = hop_count,
                timestamp = timestamp_ms_epoch / 1000.0) # Convert to seconds
            return cb(data)
        except:
            # Ignore exceptions from callbacks, TODO: Don't ignore, report
            pass
    return False


cdef void _remote_status_reception_cb_c(wpc.app_addr_t source_address,
                                        const wpc.app_scratchpad_status_t* status,
                                        uint16_t update_timeout) with gil:
    '''C callback for remote status reception'''

    global _remote_status_reception_cb
    cdef object cb

    cb = _remote_status_reception_cb
    if cb:
        version = (status.firmware_major_ver,
                   status.firmware_minor_ver,
                   status.firmware_maint_ver,
                   status.firmware_dev_ver)
        scratchpad_status = ScratchpadStatus(
            scratchpad_num_bytes = status.scrat_len,
            scratchpad_crc = status.scrat_crc,
            scratchpad_seq = status.scrat_seq_number,
            scratchpad_type = status.scrat_type,
            scratchpad_status = status.scrat_status,
            processed_num_bytes = status.processed_scrat_len,
            processed_crc = status.processed_scrat_crc,
            processed_seq = status.processed_scrat_seq_number,
            firmware_memory_area_id = status.firmware_memory_area_id,
            firmware_version = version)
        try:
            # Call the registered Python callback
            cb(source_address, scratchpad_status, update_timeout)
        except:
            # Ignore exceptions from callbacks, TODO: Don't ignore, report
            pass


cdef void _neighbor_scan_done_cb_c(uint8_t scan_ready) with gil:
    '''C callback for neighbor scan done'''

    global _neighbor_scan_done_cb
    cdef object cb

    cb = _neighbor_scan_done_cb
    if cb:
        try:
            # Call the registered Python callback
            cb(scan_ready)
        except:
            # Ignore exceptions from callbacks, TODO: Don't ignore, report
            pass


cdef void _stack_status_cb_c(uint8_t status) with gil:
    '''C callback for stack status'''

    global _stack_status_cb
    cdef object cb

    cb = _stack_status_cb
    if cb:
        try:
            # Call the registered Python callback
            cb(StackStatus(value = status))
        except:
            # Ignore exceptions from callbacks, TODO: Don't ignore, report
            pass


#### Classes

class MeshAPIError(Exception):
    '''Mesh API error'''

    _exc_map = {
        RES_OK:                         "no error",
        RES_STACK_NOT_STOPPED:          "stack not stopped",
        RES_STACK_ALREADY_STOPPED:      "stack already stopped",
        RES_STACK_ALREADY_STARTED:      "stack already started",
        RES_INVALID_VALUE:              "invalid value",
        RES_ROLE_NOT_SET:               "role not set",
        RES_NODE_ADDRESS_NOT_SET:       "node address not set",
        RES_NETWORK_ADDRESS_NOT_SET:    "network address not set",
        RES_NETWORK_CHANNEL_NOT_SET:    "network channel not set",
        RES_STACK_IS_STOPPED:           "stack is stopped",
        RES_NODE_NOT_A_SINK:            "node is not a sink",
        RES_UNKNOWN_DEST:               "unknown destination",
        RES_NO_CONFIG:                  "no app config",
        RES_ALREADY_REGISTERED:         "already registered",
        RES_NOT_REGISTERED:             "not registered",
        RES_ATTRIBUTE_NOT_SET:          "attribute not set",
        RES_ACCESS_DENIED:              "access denied",
        RES_DATA_ERROR:                 "data error",
        RES_NO_SCRATCHPAD_START:        "no scratchpad start",
        RES_NO_VALID_SCRATCHPAD:        "no valid scratchpad",
        RES_NOT_A_SINK:                 "node is not a sink", # TODO: Duplicate?
        RES_OUT_OF_MEMORY:              "out of memory",
        RES_INVALID_DIAG_INTERVAL:      "invalid diagnostic interval",
        RES_INVALID_SEQ:                "invalid sequence number",
        RES_INVALID_START_ADDRESS:      "invalid start address",
        RES_INVALID_NUMBER_OF_BYTES:    "invalid number of bytes",
        RES_INVALID_SCRATCHPAD:         "invalid scratchpad",
        RES_INVALID_REBOOT_DELAY:       "invalid reboot delay",
        RES_INTERNAL_ERROR:             "internal error",
    }

    def __init__(self, result, message = None):
        self.result = result
        if message is None:
            self.message = self._exc_map.get(result, "unknown error")
        else:
            self.message = message

    def __str__(self):
        return "%d: %s" % (self.result, self.message)


class StackStatus:
    '''Wirepas Mesh stack status'''

    def __init__(self, value = None, str_ = None):
        if value is not None:
            self.value = value
        elif str_ is not None:
            self.value = self._parse_string(str_)
        else:
            raise ValueError("must specify an integer or string value")

        self.weights = (
            (0x01, "stopped"),
            (0x02, "no network address"),
            (0x04, "no node address"),
            (0x08, "no network channel"),
            (0x10, "no role"),
            (0x20, "no app config data"),
            (0x40, "reserved6"),
            (0x80, "reserved7"),
        )

        self.names = dict([(name, weight) for weight, name in self.weights])

    def _parse_string(self, str_):
        value = 0

        try:
            str_ = str_.lower()             # Convert everything to lower case
            str_ = str_.replace("-", " ")   # Convert hyphens to spaces
            str_ = str_.replace("_", " ")   # Convert underscores to spaces
            names = str_.split(",")         # Split status string

            if len(names) == 1 and names[0] == "running":
                return 0
            else:
                # Parse bit names
                for n in names:
                    n = n.strip()
                    if n == "":
                        continue
                    weight = self.names[n]
                    value |= weight
        except (KeyError, ValueError):
            raise ValueError("invalid status string")

        return value

    def __int__(self):
        return self.value

    def __str__(self):
        names = []

        if self.value == 0:
            return "running"
        else:
            for weight, name in self.weights:
                if self.value & weight:
                    names.append(name)

        return ", ".join(names)

    def __repr__(self):
        return "StackStatus(str_ = \"%s\")" % self.__str__()


class Role:
    '''Wirepas Mesh node role'''

    def __init__(self, value = None, str_ = None):
        if value is not None:
            self.value = value
        elif str_ is not None:
            self.value = self._parse_string(str_)
        else:
            raise ValueError("must specify an integer or string value")

    def _parse_string(self, str_):
        value = 0

        try:
            str_ = str_.replace(" ", "")    # Delete spaces
            str_ = str_.replace("-", "")    # Delete hyphens ("auto-role", ...)
            str_ = str_.replace("|", "+")   # Convert pipe characters to plusses
            str_ = str_.lower()             # Convert everything to lower case

            # Split role string to base role and zero or more options
            role_str = str_.split("+")
            base_role_str = role_str[0]
            options = role_str[1:]

            # Parse base role
            if base_role_str == "sink":
                value = ROLE_SINK
            elif base_role_str == "headnode":
                value = ROLE_HEADNODE
            elif base_role_str == "subnode":
                value = ROLE_SUBNODE
            else:
                raise ValueError

            # Parse options
            for option_str in options:
                if option_str in ("ll", "lowlatency"):
                    value |= ROLE_OPTION_LOW_LATENCY
                elif option_str == "relay":
                    value |= ROLE_OPTION_RELAY
                elif option_str in ("ar", "autorole"):
                    value |= ROLE_OPTION_AUTOROLE
                else:
                    raise ValueError
        except ValueError:
            raise ValueError("invalid role string")

        return value

    def __int__(self):
        return self.value

    def __str__(self):
        role_str = []

        # Add base role name
        base_role_bits = self.value & 0x0f
        if base_role_bits == ROLE_SINK:
            role_str.append("sink")
        elif base_role_bits == ROLE_HEADNODE:
            role_str.append("headnode")
        elif base_role_bits == ROLE_SUBNODE:
            role_str.append("subnode")
        else:
            role_str.append("unknown")

        # Add options
        option_bits = self.value & 0xf0
        if option_bits & ROLE_OPTION_LOW_LATENCY:
            role_str.append("low-latency")
        if option_bits & ROLE_OPTION_RELAY:
            role_str.append("relay")
        if option_bits & ROLE_OPTION_AUTOROLE:
            role_str.append("autorole")

        return "+".join(role_str)

    def __repr__(self):
        return "Role(str_ = \"%s\")" % self.__str__()


class AppConfigData:
    '''App config data'''

    def __init__(self, seq, interval, config_bytes):
        if len(config_bytes) > APP_CONFIG_DATA_MAX_NUM_BYTES:
            raise ValueError("app config data too long: %d > %d" %
                             (len(config_bytes), APP_CONFIG_DATA_MAX_NUM_BYTES))
        self.seq = seq
        self.interval = interval
        self.config_bytes = config_bytes

    def __repr__(self):
        return "".join([
            "AppConfigData(",
            "seq = %s, "            % self.seq,
            "interval = %s, "       % self.interval,
            "config_bytes = %s)"    % repr(self.config_bytes)])


class DataToSend:
    '''Data packet to send'''

    def __init__(self,
                 bytes_,
                 dest_address,
                 on_data_sent_cb = None,
                 buffering_delay_s = 0,
                 pdu_id = 0,
                 source_endpoint = 0,
                 dest_endpoint = 0,
                 hop_limit = 0,
                 qos = QOS_NORMAL,
                 is_unack_csma_ca = False):
        self.bytes_ = bytes_
        self.dest_address = dest_address
        self.on_data_sent_cb = on_data_sent_cb
        self.buffering_delay_s = buffering_delay_s
        self.pdu_id = pdu_id
        self.source_endpoint = source_endpoint
        self.dest_endpoint = dest_endpoint
        self.hop_limit = hop_limit
        self.qos = qos
        self.is_unack_csma_ca = is_unack_csma_ca

    def __repr__(self):
        return "".join([
            "DataToSend(",
            "bytes_ = %s, "             % repr(self.bytes_),
            "dest_address = %s, "       % self.dest_address,
            "on_data_sent_cb = %s, "    % self.on_data_sent_cb,
            "buffering_delay_s = %s, "  % self.buffering_delay_s,
            "pdu_id = %s, "             % self.pdu_id,
            "source_endpoint = %s, "    % self.source_endpoint,
            "dest_endpoint = %s, "      % self.dest_endpoint,
            "hop_limit = %s, "          % self.hop_limit,
            "qos = %s, "                % self.qos,
            "is_unack_csma_ca = %s)"    % self.is_unack_csma_ca])


class ReceivedData:
    '''Received data packet'''

    def __init__(self,
                 bytes_,
                 source_address,
                 dest_address,
                 qos,
                 source_endpoint,
                 dest_endpoint,
                 travel_time_s,
                 hop_count,
                 timestamp):
        self.bytes_             = bytes_
        self.source_address     = source_address
        self.dest_address       = dest_address
        self.qos                = qos
        self.source_endpoint    = source_endpoint
        self.dest_endpoint      = dest_endpoint
        self.travel_time_s      = travel_time_s
        self.hop_count          = hop_count
        self.timestamp          = timestamp

    def __repr__(self):
        return "".join([
            "ReceivedData(",
            "bytes_ = %s, "             % repr(self.bytes_),
            "source_address = %s, "     % self.source_address,
            "dest_address = %s, "       % self.dest_address,
            "qos = %s, "                % self.qos,
            "source_endpoint = %s, "    % self.source_endpoint,
            "dest_endpoint = %s, "      % self.dest_endpoint,
            "travel_time_s = %s, "      % self.travel_time_s,
            "hop_count = %s, "          % self.hop_count,
            "timestamp = %s)"           % self.timestamp])


class ScratchpadStatus:
    '''Scratchpad status of a Wirepas Mesh node'''

    def __init__(self,
                 scratchpad_num_bytes,
                 scratchpad_crc,
                 scratchpad_seq,
                 scratchpad_type,
                 scratchpad_status,
                 processed_num_bytes,
                 processed_crc,
                 processed_seq,
                 firmware_memory_area_id,
                 firmware_version):
        self.scratchpad_num_bytes       = scratchpad_num_bytes
        self.scratchpad_crc             = scratchpad_crc
        self.scratchpad_seq             = scratchpad_seq
        self.scratchpad_type            = scratchpad_type
        self.scratchpad_status          = scratchpad_status
        self.processed_num_bytes        = processed_num_bytes
        self.processed_crc              = processed_crc
        self.processed_seq              = processed_seq
        self.firmware_memory_area_id    = firmware_memory_area_id
        self.firmware_version           = firmware_version

    def __repr__(self):
        return "\n".join([
            "ScratchpadStatus(",
            "  scratchpad_num_bytes    = %s," % repr(self.scratchpad_num_bytes),
            "  scratchpad_crc          = %s," % self.scratchpad_crc,
            "  scratchpad_seq          = %s," % self.scratchpad_seq,
            "  scratchpad_type         = %s," % self.scratchpad_type,
            "  scratchpad_status       = %s," % self.scratchpad_status,
            "  processed_num_bytes     = %s," % self.processed_num_bytes,
            "  processed_crc           = %s," % self.processed_crc,
            "  processed_seq           = %s," % self.processed_seq,
            "  firmware_memory_area_id = %s," % self.firmware_memory_area_id,
            "  firmware_version        = %s)" % self.firmware_version])


class Neighbor:
    '''A neighboring Wirepas Mesh node'''

    def __init__(self,
                 address,
                 link_rel,
                 norm_rssi,
                 cost,
                 channel,
                 neighbor_type,
                 tx_power,
                 rx_power,
                 last_update):
        self.address        = address
        self.link_rel       = link_rel
        self.norm_rssi      = norm_rssi
        self.cost           = cost
        self.channel        = channel
        self.neighbor_type  = neighbor_type
        self.tx_power       = rx_power
        self.rx_power       = tx_power
        self.last_update    = last_update

    def __repr__(self):
        return "".join([
            "Neighbor(",
            "address = %s, "        % self.address,
            "link_rel = %s, "       % self.link_rel,
            "norm_rssi = %s, "      % self.norm_rssi,
            "cost = %s, "           % self.cost,
            "channel = %s, "        % self.channel,
            "neighbor_type = %s, "  % self.neighbor_type,
            "tx_power = %s, "       % self.tx_power,
            "rx_power = %s, "       % self.rx_power,
            "last_update = %s)"     % self.last_update])


cdef class Connection:
    '''Python wrapper for Wirepas C Mesh API library'''

    cdef bool _closed


    #### Special methods

    def __cinit__(self):
        self._closed = True     # Safe initial value for __dealloc__()

    def __init__(self, const unsigned char[:] port_name, unsigned long bitrate):
        _open_connection(port_name, bitrate)
        self._closed = False

    def __dealloc__(self):
        # Close connection implicitly when deallocated (e.g. del ...)
        if not self._closed:
            _close_connection()

    def close(self):
        if not self._closed:
            _close_connection()
            self._closed = True


    #### C Mesh API wrapper methods

    def set_max_poll_fail_duration(self, int duration_s):
        _res_to_exc(wpc.WPC_set_max_poll_fail_duration(duration_s))

    def get_role(self):
        cdef wpc.app_role_t value
        _res_to_exc(wpc.WPC_get_role(&value))

        # Convert to Role object
        return Role(value = value)

    def set_role(self, object role):
        # Convert from Role object
        _res_to_exc(wpc.WPC_set_role(int(role)))

    def get_node_address(self):
        cdef wpc.app_addr_t value
        _res_to_exc(wpc.WPC_get_node_address(&value))
        return value

    def set_node_address(self, addr_t address):
        _res_to_exc(wpc.WPC_set_node_address(address))

    def get_network_address(self):
        cdef wpc.net_addr_t value
        _res_to_exc(wpc.WPC_get_network_address(&value))
        return value

    def set_network_address(self, network_addr_t address):
        _res_to_exc(wpc.WPC_set_network_address(address))

    def get_network_channel(self):
        cdef wpc.net_channel_t value
        _res_to_exc(wpc.WPC_get_network_channel(&value))
        return value

    def set_network_channel(self, network_channel_t channel):
        _res_to_exc(wpc.WPC_set_network_channel(channel))

    def get_mtu(self):
        cdef uint8_t value
        _res_to_exc(wpc.WPC_get_mtu(&value))
        return value

    def get_pdu_buffer_size(self):
        cdef uint8_t pdu_buffer_size
        _res_to_exc(wpc.WPC_get_pdu_buffer_size(&pdu_buffer_size))
        return pdu_buffer_size

    def get_scratchpad_seq(self):
        cdef uint8_t value
        _res_to_exc(wpc.WPC_get_scratchpad_sequence(&value))
        return value

    def get_mesh_API_version(self):
        cdef uint16_t value
        _res_to_exc(wpc.WPC_get_mesh_API_version(&value))
        return value

    def get_mesh_api_version(self):         # Alias for get_mesh_API_version()
        return self.get_mesh_API_version()

    def set_cipher_key(self, const uint8_t[:] key):
        if len(key) != CIPHER_KEY_NUM_BYTES:
            raise MeshAPIError(wpc.APP_RES_INVALID_VALUE, # TODO: ValueError instead?
                               "cipher key must be %d bytes" %
                               CIPHER_KEY_NUM_BYTES)
        _res_to_exc(wpc.WPC_set_cipher_key(&key[0]))

    def is_cipher_key_set(self):
        cdef bool value
        _res_to_exc(wpc.WPC_is_cipher_key_set(&value))
        return value

    def remove_cipher_key(self):
        _res_to_exc(wpc.WPC_remove_cipher_key())

    def set_authentication_key(self, const uint8_t[:] key):
        if len(key) != AUTHENTICATION_KEY_NUM_BYTES:
            raise MeshAPIError(wpc.APP_RES_INVALID_VALUE, # TODO: ValueError instead?
                               "authentication key must be %d bytes" %
                               AUTHENTICATION_KEY_NUM_BYTES)
        _res_to_exc(wpc.WPC_set_authentication_key(&key[0]))

    def is_authentication_key_set(self):
        cdef bool value
        _res_to_exc(wpc.WPC_is_authentication_key_set(&value))
        return value

    def remove_authentication_key(self):
        _res_to_exc(wpc.WPC_remove_authentication_key())

    def get_firmware_version(self):
        cdef uint16_t val[4]
        _res_to_exc(wpc.WPC_get_firmware_version(val))
        return (val[0], val[1], val[2], val[3])

    def get_channel_limits(self):
        cdef uint8_t val_min, val_max
        _res_to_exc(wpc.WPC_get_channel_limits(&val_min, &val_max))
        return (val_min, val_max)

    def do_factory_reset(self):
        _res_to_exc(wpc.WPC_do_factory_reset())

    def get_hw_magic(self):
        cdef uint16_t value
        _res_to_exc(wpc.WPC_get_hw_magic(&value))
        return value

    def get_stack_profile(self):
        cdef uint16_t value
        _res_to_exc(wpc.WPC_get_stack_profile(&value))
        return value

    def get_channel_map(self):
        cdef uint32_t value
        _res_to_exc(wpc.WPC_get_channel_map(&value))
        return value

    def set_channel_map(self, uint32_t channel_map):
        _res_to_exc(wpc.WPC_set_channel_map(channel_map))

    def start_stack(self):
        _res_to_exc(wpc.WPC_start_stack())

    def stop_stack(self):
        _res_to_exc(wpc.WPC_stop_stack())

    def get_app_config_data_size(self):
        global _app_config_data_num_bytes
        cdef uint8_t value
        _res_to_exc(wpc.WPC_get_app_config_data_size(&value))
        _app_config_data_num_bytes = value
        return value

    def set_app_config_data(self, object app_config_data):
        # Convert from app config data object
        cdef uint8_t num_bytes = len(app_config_data.config_bytes)
        cdef uint8_t config[wpc.APP_CONFIG_DATA_MAX_NUM_BYTES]
        self.get_app_config_data_size() # Update _app_config_data_num_bytes
        if num_bytes > _app_config_data_num_bytes:
            raise MeshAPIError(wpc.APP_RES_INVALID_VALUE, # TODO: ValueError instead?
                               "app config data length must be < %d bytes" %
                               _app_config_data_num_bytes)
        memset(config, 0, wpc.APP_CONFIG_DATA_MAX_NUM_BYTES)
        config[:num_bytes] = app_config_data.config_bytes
        _res_to_exc(wpc.WPC_set_app_config_data(
            app_config_data.seq,
            app_config_data.interval,
            config,
            num_bytes))

    def get_app_config_data(self):
        global _app_config_data_num_bytes
        cdef uint8_t seq
        cdef uint16_t interval
        cdef uint8_t config[wpc.APP_CONFIG_DATA_MAX_NUM_BYTES]
        cdef int num_bytes
        self.get_app_config_data_size() # Update _app_config_data_num_bytes
        _res_to_exc(wpc.WPC_get_app_config_data(
            &seq, &interval, config, _app_config_data_num_bytes))

        # Convert to app config data object
        return AppConfigData(seq = seq,
                             interval = interval,
                             config_bytes = config[:_app_config_data_num_bytes])


    def set_sink_cost(self, int cost):
        _res_to_exc(wpc.WPC_set_sink_cost(cost))

    def get_sink_cost(self):
        cdef uint8_t value
        _res_to_exc(wpc.WPC_get_sink_cost(&value))
        return value

    def register_for_app_config_data(self, object callback):
        global _app_config_data_reception_cb, _app_config_data_reception_cb_c
        self.get_app_config_data_size() # Update _app_config_data_num_bytes
        _res_to_exc(wpc.WPC_register_for_app_config_data(
            _app_config_data_reception_cb_c))
        _app_config_data_reception_cb = callback

    def unregister_from_app_config_data(self):
        global _app_config_data_reception_cb
        _app_config_data_reception_cb = None
        _res_to_exc(wpc.WPC_unregister_from_app_config_data())

    def get_stack_status(self):
        cdef uint8_t value
        _res_to_exc(wpc.WPC_get_stack_status(&value))
        return StackStatus(value = value)

    def get_PDU_buffer_usage(self):
        cdef uint8_t value
        _res_to_exc(wpc.WPC_get_PDU_buffer_usage(&value))
        return value

    def get_pdu_buffer_usage(self):     # Alias for get_PDU_buffer_usage()
        return self.get_PDU_buffer_usage()

    def get_PDU_buffer_capacity(self):
        cdef uint8_t value
        _res_to_exc(wpc.WPC_get_PDU_buffer_capacity(&value))
        return value

    def get_pdu_buffer_capacity(self):  # Alias for get_PDU_buffer_capacity()
        return self.get_PDU_buffer_capacity()

    def get_remaining_energy(self):
        cdef uint8_t value
        _res_to_exc(wpc.WPC_get_remaining_energy(&value))
        return value

    def set_remaining_energy(self, uint8_t energy):
        _res_to_exc(wpc.WPC_set_remaining_energy(energy))

    def get_autostart(self):
        cdef uint8_t value
        _res_to_exc(wpc.WPC_get_autostart(&value))
        return value and True or False

    def set_autostart(self, bool enable):
        _res_to_exc(wpc.WPC_set_autostart(enable))

    def get_route_count(self):
        cdef uint8_t value
        _res_to_exc(wpc.WPC_get_route_count(&value))
        return value

    def get_system_time(self):
        cdef uint32_t value
        _res_to_exc(wpc.WPC_get_system_time(&value))
        return value

    def get_access_cycle_range(self):
        cdef uint16_t val_min, val_max
        _res_to_exc(wpc.WPC_get_access_cycle_range(&val_min, &val_max))
        return (val_min, val_max)

    def set_access_cycle_range(self, uint16_t min_ac, uint16_t max_ac):
        _res_to_exc(wpc.WPC_set_access_cycle_range(min_ac, max_ac))

    def get_access_cycle_limits(self):
        cdef uint16_t val_min, val_max
        _res_to_exc(wpc.WPC_get_access_cycle_limits(&val_min, &val_max))
        return (val_min, val_max)

    def get_current_access_cycle(self):
        cdef uint16_t value
        _res_to_exc(wpc.WPC_get_current_access_cycle(&value))
        return value

    def get_scratchpad_block_max(self):
        cdef uint8_t value
        _res_to_exc(wpc.WPC_get_scratchpad_block_max(&value))
        return value

    def get_local_scratchpad_status(self):
        cdef wpc.app_scratchpad_status_t value
        _res_to_exc(wpc.WPC_get_local_scratchpad_status(&value))
        return ScratchpadStatus(
            scratchpad_num_bytes    = value.scrat_len,
            scratchpad_crc          = value.scrat_crc,
            scratchpad_seq          = value.scrat_seq_number,
            scratchpad_type         = value.scrat_type,
            scratchpad_status       = value.scrat_status,
            processed_num_bytes     = value.processed_scrat_len,
            processed_crc           = value.processed_scrat_crc,
            processed_seq           = value.processed_scrat_seq_number,
            firmware_memory_area_id = value.firmware_memory_area_id,
            firmware_version        = (value.firmware_major_ver,
                                       value.firmware_minor_ver,
                                       value.firmware_maint_ver,
                                       value.firmware_dev_ver))

    def start_local_scratchpad_update(self, uint32_t num_bytes, uint8_t seq):
        _res_to_exc(wpc.WPC_start_local_scratchpad_update(num_bytes, seq))

    def upload_local_block_scratchpad(self,
                                      const uint8_t[:] bytes_,
                                      uint32_t start):
        _res_to_exc(wpc.WPC_upload_local_block_scratchpad(len(bytes_),
                                                          &bytes_[0],
                                                          start))

    def upload_local_scratchpad(self, const uint8_t[:] bytes_, uint8_t seq):
        _res_to_exc(wpc.WPC_upload_local_scratchpad(len(bytes_),
                                                    &bytes_[0],
                                                    seq))

    def clear_local_scratchpad(self):
        _res_to_exc(wpc.WPC_clear_local_scratchpad())

    def update_local_scratchpad(self):
        _res_to_exc(wpc.WPC_update_local_scratchpad())

    def get_remote_status(self, addr_t address):
        _res_to_exc(wpc.WPC_get_remote_status(address))

    def remote_scratchpad_update(self,
                                 addr_t address,
                                 uint8_t seq,
                                 uint16_t reboot_delay_s):
        _res_to_exc(wpc.WPC_remote_scratchpad_update(address,
                                                     seq,
                                                     reboot_delay_s))

    def start_scan_neighbors(self):
        _res_to_exc(wpc.WPC_start_scan_neighbors())

    def get_neighbors(self):
        cdef wpc.app_nbors_t nbors_list
        cdef wpc.app_nbor_info_t* nbor
        cdef unsigned int num_nbors
        cdef unsigned int n
        _res_to_exc(wpc.WPC_get_neighbors(&nbors_list))
        num_nbors = nbors_list.number_of_neighbors
        neighbors = [None] * num_nbors
        for n in range(nbors_list.number_of_neighbors):
            nbor = &nbors_list.nbors[n]
            neighbors[n] = Neighbor(
                address         = nbor.add,
                link_rel        = nbor.link_rel,
                norm_rssi       = nbor.norm_rssi,
                cost            = nbor.cost,
                channel         = nbor.channel,
                neighbor_type   = nbor.nbor_type,
                tx_power        = nbor.tx_power,
                rx_power        = nbor.rx_power,
                last_update     = nbor.last_update)
        return neighbors

    def send_data(self, object data_to_send):
        cdef wpc.app_message_t value
        value.bytes             = data_to_send.bytes_
        value.dst_addr          = data_to_send.dest_address
        value.on_data_sent_cb   = NULL # TODO: data_to_send.on_data_sent_cb
        value.buffering_delay   = (
            int(data_to_send.buffering_delay_s * 1000)) # Convert to ms
        value.pdu_id            = data_to_send.pdu_id
        value.num_bytes         = len(data_to_send.bytes_)
        value.src_ep            = data_to_send.source_endpoint
        value.dst_ep            = data_to_send.dest_endpoint
        value.hop_limit         = data_to_send.hop_limit
        value.qos               = data_to_send.qos
        value.is_unack_csma_ca  = data_to_send.is_unack_csma_ca
        _res_to_exc(wpc.WPC_send_data_with_options(&value))

    def register_for_data(self, uint8_t dest_endpoint, object callback):
        global _data_reception_cbs, _data_reception_cb_c
        _res_to_exc(wpc.WPC_register_for_data(dest_endpoint,
                                              _data_reception_cb_c))
        _data_reception_cbs[dest_endpoint] = callback

    def unregister_for_data(self, uint8_t dest_endpoint):
        global _data_reception_cbs
        del _data_reception_cbs[dest_endpoint]
        _res_to_exc(wpc.WPC_unregister_for_data(dest_endpoint))

    def register_for_remote_status(self, object callback):
        global _remote_status_reception_cb, _remote_status_reception_cb_c
        _res_to_exc(wpc.WPC_register_for_remote_status(
            _remote_status_reception_cb_c))
        _remote_status_reception_cb = callback

    def unregister_for_remote_status(self):
        global _remote_status_reception_cb
        _remote_status_reception_cb = None
        _res_to_exc(wpc.WPC_unregister_for_remote_status())

    def register_for_scan_neighbors_done(self, object callback):
        global _neighbor_scan_done_cb, _neighbor_scan_done_cb_c
        _res_to_exc(wpc.WPC_register_for_scan_neighbors_done(
            _neighbor_scan_done_cb_c))
        _neighbor_scan_done_cb = callback

    def unregister_from_scan_neighbors_done(self):
        global _neighbor_scan_done_cb
        _neighbor_scan_done_cb = None
        _res_to_exc(wpc.WPC_unregister_from_scan_neighbors_done())

    def register_for_stack_status(self, object callback):
        global _stack_status_cb, _stack_status_cb_c
        _res_to_exc(wpc.WPC_register_for_stack_status(_stack_status_cb_c))
        _stack_status_cb = callback

    def unregister_from_stack_status(self):
        global _stack_status_cb
        _stack_status_cb = None
        _res_to_exc(wpc.WPC_unregister_from_stack_status())
