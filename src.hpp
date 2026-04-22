#ifndef SRC_HPP
#define SRC_HPP

// don't include other headfiles
#include <string>
#include <vector>
#include <set>

class Location {
public:
    virtual std::string show() const = 0;
    virtual int getId() const = 0;
};

class Register : public Location {
private:
    int id;
public:
    Register(int regId) : id(regId) {}
    virtual std::string show() const { return std::string("reg") + std::to_string(id); }
    virtual int getId() const { return id; }
};

class StackSlot : public Location {
public:
    StackSlot() {}
    virtual std::string show() const { return std::string("stack"); }
    virtual int getId() const { return -1; }
};

struct LiveInterval {
    int startpoint;
    int endpoint;
    Location* location = nullptr;
};

class LinearScanRegisterAllocator {
private:
    struct CmpEnd {
        bool operator()(const LiveInterval* a, const LiveInterval* b) const {
            if (a->endpoint != b->endpoint) return a->endpoint < b->endpoint;
            return a < b;
        }
    };

    std::set<LiveInterval*, CmpEnd> active;
    std::vector<int> freed_stack;           // FILO of freed registers
    std::set<int> never_used;               // registers never allocated before

    void releaseIntervalRegister(LiveInterval* itv) {
        if (itv && itv->location) {
            int rid = itv->location->getId();
            if (rid >= 0) {
                // push to freed stack (was allocated before)
                freed_stack.push_back(rid);
            }
        }
    }

    void expireOldIntervals(LiveInterval& i) {
        while (!active.empty()) {
            auto it = active.begin();
            LiveInterval* cur = *it;
            if (cur->endpoint >= i.startpoint) break;
            // interval expired before i starts
            releaseIntervalRegister(cur);
            active.erase(it);
        }
    }

    void spillAtInterval(LiveInterval& i) {
        // among active U {i}, spill the one with the largest endpoint
        if (active.empty()) {
            // no active, so spill i itself
            i.location = new StackSlot();
            return;
        }
        auto it_last = active.end();
        --it_last;
        LiveInterval* spill = *it_last;
        if (spill->endpoint > i.endpoint) {
            // spill the active interval with the largest end, give its reg to i
            int rid = spill->location->getId();
            spill->location = new StackSlot();
            active.erase(it_last);
            i.location = new Register(rid);
            active.insert(&i);
        } else {
            // spill i itself
            i.location = new StackSlot();
        }
    }

    int acquireFreeRegister() {
        if (!freed_stack.empty()) {
            int rid = freed_stack.back();
            freed_stack.pop_back();
            return rid;
        }
        // use smallest id from never_used
        auto it = never_used.begin();
        int rid = *it;
        never_used.erase(it);
        return rid;
    }

public:
    LinearScanRegisterAllocator(int regNum) {
        for (int r = 0; r < regNum; ++r) never_used.insert(r);
    }

    void linearScanRegisterAllocate(std::vector<LiveInterval>& intervalList) {
        // intervalList is sorted by startpoint ascending; endpoints are unique
        active.clear();
        freed_stack.clear();
        // never_used is initialized in constructor and persists
        for (auto& itv : intervalList) {
            expireOldIntervals(itv);
            bool has_free = (!freed_stack.empty()) || (!never_used.empty());
            if (has_free) {
                int rid = acquireFreeRegister();
                itv.location = new Register(rid);
                active.insert(&itv);
            } else {
                spillAtInterval(itv);
            }
        }
    }
};

#endif
