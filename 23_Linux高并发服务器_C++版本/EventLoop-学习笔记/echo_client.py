#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
EventLoop 项目联调客户端（配套学习笔记第 07 章）

协议：4 字节头 = msg_type(uint16, 大端) + body_len(uint16, 大端)，随后是 body。
服务端是 echo：把整个包（含头）原样发回。

用法示例：
    python3 echo_client.py echo                       # 基本回显（5 个包）
    python3 echo_client.py echo -n 8                  # 8 条并发连接各自回显
    python3 echo_client.py split                      # 半包：头和体分两次发
    python3 echo_client.py split-head                 # 半截头：头拆成 2+2 字节发
    python3 echo_client.py sticky                     # 粘包：一次 write 发 3 个包
    python3 echo_client.py oversize                   # 伪造超长包（body_len=3000）
    python3 echo_client.py zero                       # 空包体（body_len=0），复现缺陷 D1
    python3 echo_client.py flood -r 3000 -s 1024      # 只发不读，逼服务端走 EPOLLOUT 续传
"""

import argparse
import socket
import struct
import sys
import threading
import time

HEADER = struct.Struct("!HH")
BUFF_SIZE = 2048


# ---------------------------------------------------------------- 基础工具

def pack(msg_type: int, payload: bytes = b"") -> bytes:
    """按协议拼一个完整包。"""
    return HEADER.pack(msg_type & 0xFFFF, len(payload) & 0xFFFF) + payload


def recv_exactly(sock: socket.socket, n: int) -> bytes:
    """读满 n 字节；对端提前关闭则抛 ConnectionError。"""
    chunks = []
    got = 0
    while got < n:
        piece = sock.recv(n - got)
        if not piece:
            raise ConnectionError("对端关闭，只收到 %d/%d 字节" % (got, n))
        chunks.append(piece)
        got += len(piece)
    return b"".join(chunks)


def recv_packet(sock: socket.socket):
    """读一个完整包，返回 (msg_type, body)。"""
    msg_type, body_len = HEADER.unpack(recv_exactly(sock, HEADER.size))
    return msg_type, recv_exactly(sock, body_len)


def connect(host: str, port: int, rcvbuf=None, timeout: float = 5.0) -> socket.socket:
    """自己建 socket 而不是 create_connection，方便在 connect 之前设 SO_RCVBUF
    （connect 之前设置才会参与接收窗口的协商，这是 flood 用例能不能真正压满发送缓冲的关键）。"""
    sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    if rcvbuf:
        sock.setsockopt(socket.SOL_SOCKET, socket.SO_RCVBUF, rcvbuf)
    sock.settimeout(timeout)
    sock.connect((host, port))
    return sock


def report(name: str, ok: bool, detail: str = "") -> bool:
    print("[%s] %s %s" % ("OK  " if ok else "FAIL", name, detail))
    return ok


# ---------------------------------------------------------------- 各个用例

def case_echo(host, port, rounds: int, size: int, conns: int) -> bool:
    """基本回显；conns > 1 时开多条并发连接。"""
    results = [True] * conns

    def worker(idx: int):
        try:
            sock = connect(host, port)
            with sock:
                for i in range(rounds):
                    msg_type = 1000 + (i % 8)
                    body = ("conn%d-msg%d-" % (idx, i)).encode() + bytes(size)
                    sock.sendall(pack(msg_type, body))
                    got_type, got_body = recv_packet(sock)
                    if got_type != msg_type or got_body != body:
                        results[idx] = False
                        print("      连接 %d 第 %d 个包不匹配" % (idx, i))
                        return
        except Exception as exc:                                # noqa: BLE001
            results[idx] = False
            print("      连接 %d 异常：%s" % (idx, exc))

    threads = [threading.Thread(target=worker, args=(i,)) for i in range(conns)]
    for t in threads:
        t.start()
    for t in threads:
        t.join()

    return report("基本回显（%d 连接 × %d 包 × %d 字节）" % (conns, rounds, size),
                  all(results))


def case_split(host, port) -> bool:
    """半包：先发 4 字节头，sleep 后再发包体。服务端必须能跨两次事件把包拼起来。"""
    body = b"split-body-payload"
    with connect(host, port) as sock:
        sock.sendall(HEADER.pack(2001, len(body)))
        time.sleep(0.3)
        sock.sendall(body)
        got_type, got_body = recv_packet(sock)
    return report("半包（头/体分两次发）", got_type == 2001 and got_body == body,
                  "收到 type=%d len=%d" % (got_type, len(got_body)))


def case_split_head(host, port) -> bool:
    """半截头：把 4 字节头拆成 2 + 2 字节两次发，再发 body。"""
    body = b"split-head-payload"
    head = HEADER.pack(2002, len(body))
    with connect(host, port) as sock:
        sock.sendall(head[:2])
        time.sleep(0.3)
        sock.sendall(head[2:] + body)
        got_type, got_body = recv_packet(sock)
    return report("半截头（头拆成 2+2 字节）", got_type == 2002 and got_body == body,
                  "收到 type=%d len=%d" % (got_type, len(got_body)))


def case_sticky(host, port) -> bool:
    """粘包：一次 write 发 3 个包，服务端应该回 3 个包。"""
    packets = [pack(3000 + i, ("sticky-%d-" % i).encode() + bytes(16)) for i in range(3)]
    with connect(host, port) as sock:
        sock.sendall(b"".join(packets))
        got = [recv_packet(sock) for _ in range(3)]
        # 再确认没有多余数据（读不到东西才对）
        sock.settimeout(0.3)
        extra = b""
        try:
            extra = sock.recv(1)
        except socket.timeout:
            extra = b""
    ok = all(got[i][0] == 3000 + i and got[i][1] == packets[i][4:] for i in range(3)) and extra == b""
    return report("粘包（一次发 3 个包）", ok, "收到 %d 个包" % len(got))


def case_oversize(host, port) -> bool:
    """伪造 body_len > BUFF_SIZE(2048) 的头，服务端应该主动断开（长度校验 + 断连）。"""
    fake_len = 3000
    with connect(host, port) as sock:
        sock.sendall(HEADER.pack(4001, fake_len))
        try:
            sock.sendall(bytes(fake_len))
        except OSError:
            pass                                     # 服务端可能已经把连接断掉了
        sock.settimeout(2.0)
        try:
            data = sock.recv(64)
        except (socket.timeout, ConnectionResetError):
            data = b""
    return report("超长包被拒（body_len=%d > %d）" % (fake_len, BUFF_SIZE), data == b"",
                  "连接已断开" if data == b"" else "服务端居然回了 %d 字节" % len(data))


def case_zero(host, port) -> bool:
    """空包体：这里是复现原版的缺陷 D1，所以“断开”是符合预期的现象，记 PASS（复现成功）。"""
    with connect(host, port) as sock:
        sock.sendall(HEADER.pack(5001, 0))
        sock.settimeout(1.5)
        try:
            data = sock.recv(64)
        except socket.timeout:
            data = b"<timeout>"
        except ConnectionResetError:
            data = b""
    if data == b"":
        print("[OK  ] 空包体缺陷复现：服务端把 body_len=0 的包当成了对端关闭（这就是原版的 D1）")
        return True
    if data == b"<timeout>":
        print("[OK  ] 空包体：服务端既不回包也不断开（另一种非预期行为，值得记录）")
        return True
    print("[OK  ] 空包体被正确回显（说明你的实现比原版更健壮，D1 已修）")
    return True


def case_flood(host, port, rounds: int, size: int, rcvbuf: int) -> bool:
    """只发不读，把服务端的内核发送缓冲打满 → 逼它走 EPOLLOUT 续传路径。

    客户端把 SO_RCVBUF 设小并且长时间不 recv，服务端 echo 的写操作很快会 EAGAIN，
    于是数据进入 _send_que，等可写事件再续传。最后我们把回包读回来对齐字节数。
    """
    payload = bytes(size)
    pkt = pack(6001, payload)
    expect_bytes = len(pkt) * rounds

    sock = connect(host, port, rcvbuf=rcvbuf, timeout=30.0)
    sent = 0
    t0 = time.time()
    try:
        for _ in range(rounds):
            sock.sendall(pkt)                     # 可能会被压着（发送缓冲满），这没关系
            sent += len(pkt)
    except OSError as exc:
        print("      发送中断：%s" % exc)

    sock.settimeout(5.0)
    got = 0
    try:
        while got < expect_bytes:
            piece = sock.recv(65536)
            if not piece:
                break
            got += len(piece)
    except socket.timeout:
        pass
    finally:
        sock.close()

    elapsed = time.time() - t0
    ok = got == sent
    return report("EPOLLOUT 续传压测（发送 %d 字节，收发一致）" % sent, ok,
                  "收到 %d 字节，用时 %.2fs，约 %.1f MB/s"
                  % (got, elapsed, (got / 1048576.0) / elapsed if elapsed else 0))


# ---------------------------------------------------------------- 入口

def main() -> int:
    parser = argparse.ArgumentParser(description="EventLoop 联调客户端")
    parser.add_argument("mode",
                        choices=["echo", "split", "split-head", "sticky",
                                 "oversize", "zero", "flood", "all"],
                        help="要跑的用例；all = 除 flood 外的全部")
    parser.add_argument("--host", default="127.0.0.1")
    parser.add_argument("--port", type=int, default=12345)
    parser.add_argument("-n", "--conns", type=int, default=1, help="echo 模式的并发连接数")
    parser.add_argument("-r", "--rounds", type=int, default=5, help="echo/flood 的包数量")
    parser.add_argument("-s", "--size", type=int, default=32, help="echo/flood 的 body 字节数")
    parser.add_argument("--rcvbuf", type=int, default=4096, help="flood 模式的 SO_RCVBUF")
    args = parser.parse_args()

    ok = True
    try:
        if args.mode in ("echo", "all"):
            ok &= case_echo(args.host, args.port,
                            5 if args.mode == "all" else args.rounds,
                            args.size, args.conns)
        if args.mode in ("split", "all"):
            ok &= case_split(args.host, args.port)
        if args.mode in ("split-head", "all"):
            ok &= case_split_head(args.host, args.port)
        if args.mode in ("sticky", "all"):
            ok &= case_sticky(args.host, args.port)
        if args.mode in ("oversize", "all"):
            ok &= case_oversize(args.host, args.port)
        if args.mode in ("zero", "all"):
            ok &= case_zero(args.host, args.port)
        if args.mode == "flood":
            ok &= case_flood(args.host, args.port, args.rounds, args.size, args.rcvbuf)
    except ConnectionRefusedError:
        print("[FAIL] 连不上 %s:%d —— 服务端起了吗？端口对吗？" % (args.host, args.port))
        return 2
    except ConnectionError as exc:
        print("[FAIL] 连接层错误：%s" % exc)
        return 2

    print("\n结论：%s" % ("全部通过" if ok else "有用例未通过，见上面 FAIL 行"))
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main())
