"""Explicit finite priors and exact (non-Monte-Carlo) cylinder coverage evaluation."""
from __future__ import annotations
from functools import lru_cache
from itertools import product, accumulate
from bisect import bisect_right
from math import fsum, isfinite, prod
from native_sampling import compile_draw, native_draw

Region = tuple[tuple[int, int], ...]


class CoverageBudgetExceeded(Exception):
    """Internal abort: unfinished recursion results are never memoized."""


def region(bindings) -> Region:
    entries = tuple(sorted(dict(bindings).items()))
    if any(type(i) is not int or i < 0 or type(v) is not int for i, v in entries):
        raise ValueError('Regions require nonnegative integer coordinates and integer values')
    return entries


def matches(r: Region, x: tuple[int, ...]) -> bool:
    return all(x[i] == v for i, v in r)


def contains(outer: Region, inner: Region) -> bool:
    """G(inner) subset G(outer)."""
    return set(outer).issubset(inner)


class ProductPrior:
    def __init__(self, names, domains, weights=None):
        self.names = tuple(names)
        self.domains = tuple(tuple(d) for d in domains)
        if not self.names or len(self.names) != len(self.domains) or len(set(self.names)) != len(self.names):
            raise ValueError('Unique names and matching nonempty domains required')
        if any(not d or len(set(d)) != len(d) or any(type(v) is not int for v in d) for d in self.domains):
            raise ValueError('Nonempty unique integer domains required')
        if weights is None:
            weights = [[1] * len(d) for d in self.domains]
        if len(weights) != len(self.domains):
            raise ValueError('Weights/domains dimension mismatch')
        probabilities = []
        for domain, ws in zip(self.domains, weights):
            if len(ws) != len(domain) or any(not isfinite(w) or w < 0 for w in ws) or fsum(ws) <= 0:
                raise ValueError('Finite nonnegative weights with positive total required')
            probabilities.append(tuple(w / fsum(ws) for w in ws))
        self.probabilities = tuple(probabilities)
        # Match random.choices exactly: ordinary cumulative addition, then
        # bisect_right with the last element excluded. Do not use fsum here.
        self._draw_specs = tuple((d, tuple(accumulate(p))) for d, p in zip(self.domains, self.probabilities))
        self._native_draw = compile_draw(self._draw_specs)
        self.maps = tuple(dict(zip(d, p)) for d, p in zip(self.domains, self.probabilities))
        self.size = prod(map(len, self.domains))
        self.mass_node_limit=0  # Zero keeps the original exact computation.
        self.coverage_fallbacks=0;self._mass_nodes=0
        # Residual unions recur across candidate regions AND cache revisions.
        # Bound retention; probabilities are immutable for this prior instance.
        self._residual_mass = lru_cache(maxsize=32768)(self._visit_uncovered)

    def _visit_uncovered(self, regions):
        if self.mass_node_limit:
            self._mass_nodes+=1
            if self._mass_nodes>self.mass_node_limit:raise CoverageBudgetExceeded
        if not regions:
            return 1.0
        if () in regions:
            return 0.0
        coordinate = min(i for r in regions for i, _ in r)
        values = sorted({v for r in regions for i, v in r if i == coordinate})
        total, assigned = 0.0, 0.0
        for value in values:
            probability = self.maps[coordinate].get(value, 0.0)
            assigned += probability
            children = []
            for r in regions:
                bindings = dict(r)
                if coordinate not in bindings or bindings[coordinate] == value:
                    children.append(tuple((i, v) for i, v in r if i != coordinate))
            total += probability * self._residual_mass(tuple(sorted(set(children))))
        other = tuple(r for r in regions if coordinate not in dict(r))
        total += max(0.0, 1.0 - assigned) * self._residual_mass(tuple(sorted(set(other))))
        return total

    def draw(self, rng):
        if self._native_draw is not None:
            return native_draw(self._native_draw, rng.random)
        random = rng.random
        return tuple(d[bisect_right(c, random() * c[-1], 0, len(d)-1)] for d, c in self._draw_specs)

    def pmf(self, x):
        return prod(p.get(v, 0.0) for p, v in zip(self.maps, x)) if len(x) == len(self.domains) else 0.0

    def points(self):
        return product(*self.domains)

    def uncovered(self, candidate: Region, cached: tuple[Region, ...]) -> float:
        """Uncovered mass, or its upper bound if the optional budget expires.

        Unmentioned values are grouped together. Does not enumerate the Cartesian
        input space. Floating point is used for scores, never as a safety proof.
        """
        fixed = dict(candidate)
        mass = prod(self.maps[i].get(v, 0.0) for i, v in candidate)
        if mass == 0:
            return 0.0
        compatible = []
        for old in cached:
            if all(i not in fixed or fixed[i] == v for i, v in old):
                compatible.append(tuple((i, v) for i, v in old if i not in fixed))
        self._mass_nodes=0
        try:return max(0.0, mass * self._residual_mass(tuple(sorted(set(compatible)))))
        except CoverageBudgetExceeded:
            # pi(G(candidate)) bounds its uncovered part from above. It affects
            # ranking/eviction only; it never certifies a feasibility answer.
            self.coverage_fallbacks+=1
            return mass

    def metadata(self):
        return dict(kind='product', names=self.names, domains=self.domains, probabilities=self.probabilities, size=self.size)


class JointPrior(ProductPrior):
    def __init__(self, names, domains, atoms):
        super().__init__(names, domains)
        if not atoms or any(len(x) != len(self.names) or any(v not in d for v, d in zip(x, self.domains)) for x in atoms):
            raise ValueError('Joint atoms must belong to declared domains')
        if any(not isfinite(w) or w < 0 for w in atoms.values()) or fsum(atoms.values()) <= 0:
            raise ValueError('Invalid joint weights')
        total = fsum(atoms.values())
        self.atoms = {tuple(x): w / total for x, w in atoms.items() if w > 0}
        self.population, self.weights = tuple(self.atoms), tuple(self.atoms.values())
        self._joint_cumulative = tuple(accumulate(self.weights))
        self.size = len(self.atoms)

    def draw(self, rng):
        return self.population[bisect_right(self._joint_cumulative,
            rng.random()*self._joint_cumulative[-1], 0, len(self.population)-1)]

    def points(self):
        return iter(self.atoms)

    def pmf(self, x):
        return self.atoms.get(tuple(x), 0.0)

    def uncovered(self, candidate, cached):
        return fsum(p for x, p in self.atoms.items() if matches(candidate, x) and not any(matches(c, x) for c in cached))

    def metadata(self):
        return dict(kind='joint', names=self.names, domains=self.domains, size=self.size,
                    atoms=[dict(x=x, probability=p) for x, p in self.atoms.items()])
