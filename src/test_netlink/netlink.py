# coding: utf-8

import _netlink

NETLINK_PROTOCOL = 30

DEFAULT_PID = 1
DEFAULT_GROUP = 1

DEFAULT_DEST_PID = 0
DEFAULT_DEST_GROUP = 0

DEFAULT_SEND_TYPE = 0
DEFAULT_RECV_TYPE = 0


def create(pid=DEFAULT_PID, group=DEFAULT_GROUP, protocol=NETLINK_PROTOCOL):
    """Create a Netlink Socket.

    If argument error, return -1; if the linux kernel failed to create the
    netlink socket, return -2; if the file description is more than the max
    value, return -3; If successfully, return a positive number.
    """
    return _netlink.create(pid=pid, group=group, protocol=protocol)


def recv(fd, type=DEFAULT_RECV_TYPE):
    """Return a tuple, that's, (data, size, type, flags, seq, pid)."""
    return _netlink.recv(fd, type)


def send(fd, data, size, type=DEFAULT_SEND_TYPE, pid=DEFAULT_DEST_PID, group=DEFAULT_DEST_GROUP):
    """Return the byte number sent in fact. If failed, return a negative number."""
    return _netlink.send(fd, data, size, pid, group, type)


def close(fd):
    """Return a None."""
    _netlink.close(fd)


class Netlink(object):
    def __init__(self, pid=DEFAULT_PID, group=DEFAULT_GROUP,
                 dst_pid=DEFAULT_DEST_PID, dst_group=DEFAULT_DEST_GROUP,
                 protocol=NETLINK_PROTOCOL):
        self.pid = pid
        self.group = group
        self.dst_pid = dst_pid
        self.dst_group = dst_group
        self._protocol = protocol
        self._fd = create(self.pid, self.group, self._protocol)
        if self._fd == -1:
            raise Exception("The argument is error")
        elif self._fd == -2:
            raise Exception("The kernel failed to create the netlink socket")
        elif self._fd == -3:
            raise Exception("more than the max file description")

    def __del__(self):
        self.close()

    def recv(self, type=DEFAULT_RECV_TYPE):
        return recv(self._fd, type)

    def send(self, data, size, type=DEFAULT_SEND_TYPE, pid=None, group=None):
        if pid is None:
            pid = self.dst_pid
        if group is None:
            group = self.dst_group
        return send(self._fd, data, size, type, pid, group)

    def close(self):
        if self._fd != -1:
            close(self._fd)


if __name__ == "__main__":
    fd = Netlink()
    print "Create a Netlink Socket"
    fd.close()
    print "Close a Netlink Socket"
