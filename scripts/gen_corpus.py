#!/usr/bin/env python3
"""Emit a CSV corpus of `phrase,weight` lines for serving_node manual testing."""
import argparse, random, string, sys

def main() -> int:
    p = argparse.ArgumentParser()
    p.add_argument("-n", type=int, default=1000)
    p.add_argument("--seed", type=int, default=0)
    args = p.parse_args()

    rng = random.Random(args.seed)
    vocab = ["revenue", "region", "sales", "customer", "order", "product",
             "quantity", "total", "quarter", "year", "margin", "forecast"]
    for _ in range(args.n):
        k = rng.randint(1, 3)
        phrase = " ".join(rng.choice(vocab) for _ in range(k))
        w = round(rng.random() * 10, 3)
        print(f"{phrase},{w}")
    return 0

if __name__ == "__main__":
    sys.exit(main())
