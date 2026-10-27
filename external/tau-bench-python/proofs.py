"""Proof lookup for one immutable CP model.

UNSAT patterns are the live, capacity-controlled nogood cache. Unary and binary
patterns have direct indexes; longer patterns watch two negated equality
literals. Watches detect conflict under independent partial assignments; they
do not propagate or repair sampled values. SAT records are actual solutions
(or their certified partial assumptions), indexed by literal bitsets.
"""
from dataclasses import dataclass


@dataclass(frozen=True)
class SatProof:
    bindings: tuple
    witness: dict
    completion: tuple | None


class NogoodIndex:
    def __init__(self, mode='watched'):
        if mode not in ('watched','linear'):
            raise ValueError('Unknown nogood index')
        self.mode=mode
        self.lookups=self.watch_visits=self.watch_moves=0
        self.replace(())

    def replace(self, regions):
        self.regions=tuple(regions)
        self.empty=() if () in self.regions else None
        self.units={};self.binary={};self.long=[];self.positions=[];self.watches={}
        if self.mode=='linear':return
        for region in self.regions:
            if len(region)==1:self.units[region[0]]=region
            elif len(region)==2:self.binary.setdefault(region[0],{})[region[1]]=region
            elif len(region)>2:
                index=len(self.long);self.long.append(region);self.positions.append([0,1])
                for literal in region[:2]:self.watches.setdefault(literal,[]).append(index)

    def find(self, bindings):
        self.lookups+=1
        if self.empty is not None:return self.empty
        if not self.regions:return None
        values=dict(bindings)
        if self.mode=='linear':
            return next((r for r in self.regions if all(i in values and values[i]==v for i,v in r)),None)
        for literal in bindings:
            if literal in self.units:return self.units[literal]
        literals=set(bindings)
        for literal in bindings:
            for other,region in self.binary.get(literal,{}).items():
                if other in literals:return region
        # A watched clause literal not(X_i=v) is false exactly when (i,v)
        # occurs in this query. A replacement is never false in this query.
        for literal in bindings:
            watchlist=self.watches.get(literal,[]);cursor=0
            while cursor<len(watchlist):
                self.watch_visits+=1
                index=watchlist[cursor];region=self.long[index];positions=self.positions[index]
                slot=0 if region[positions[0]]==literal else 1
                other_slot=1-slot;other=region[positions[other_slot]]
                if other[0] in values and values[other[0]]!=other[1]:
                    cursor+=1;continue  # Clause already satisfied.
                replacement=next((j for j,binding in enumerate(region)
                                  if j not in positions and binding not in literals),None)
                if replacement is not None:
                    positions[slot]=replacement
                    watchlist[cursor]=watchlist[-1];watchlist.pop()
                    self.watches.setdefault(region[replacement],[]).append(index)
                    self.watch_moves+=1
                elif other in literals:
                    return region
                else:
                    cursor+=1  # Unit, but no propagation is performed here.
        return None


class ProofStore:
    def __init__(self, index='watched'):
        self.nogoods=NogoodIndex(index)
        self.sat=[];self.sat_exact={};self.sat_bits={};self.all_sat=0

    def remember_sat(self, bindings, witness, completion=None):
        certified=tuple(enumerate(completion)) if completion is not None else tuple(bindings)
        if certified in self.sat_exact:return
        index=len(self.sat);bit=1<<index
        proof=SatProof(certified,dict(witness),tuple(completion) if completion is not None else None)
        self.sat.append(proof);self.sat_exact[certified]=index;self.all_sat|=bit
        for literal in certified:self.sat_bits[literal]=self.sat_bits.get(literal,0)|bit

    def find_sat(self, bindings):
        if bindings in self.sat_exact:return self.sat[self.sat_exact[bindings]]
        possible=self.all_sat
        for literal in bindings:
            possible&=self.sat_bits.get(literal,0)
            if not possible:return None
        if not possible:return None
        index=(possible & -possible).bit_length()-1
        return self.sat[index]

    def stats(self):
        n=self.nogoods
        return dict(sat_entries=len(self.sat),nogood_entries=len(n.regions),
                    nogood_lookups=n.lookups,watch_visits=n.watch_visits,watch_moves=n.watch_moves)
