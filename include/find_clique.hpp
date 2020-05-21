#include "util.hpp"
#include "cell_cache.hpp"
#include "bundle_cache.hpp"
#include "clique_cache.hpp"
#include "small_cliques.hpp"
#include "topo_cache.hpp"


namespace busclique {


template<typename T>
size_t get_maxlen(vector<T> &emb, size_t size) {
    auto cmp = [](T a, T b) { return a.size() < b.size(); };
    std::sort(emb.begin(), emb.end(), cmp);
    return emb[size-1].size();
}

template<typename topo_spec>
bool find_clique_nice(const cell_cache<topo_spec> &,
                      size_t size,
                      vector<vector<size_t>> &emb,
                      size_t &max_length);

template<>
bool find_clique_nice(const cell_cache<chimera_spec> &cells,
                      size_t size,
                      vector<vector<size_t>> &emb,
                      size_t &max_length) {
    bundle_cache<chimera_spec> bundles(cells);
    size_t shore = cells.topo.shore;
    if(size <= shore)
        for(size_t y = 0; y < cells.topo.dim[0]; y++)
            for(size_t x = 0; x < cells.topo.dim[1]; x++)
                if (bundles.score(y,x,y,y,x,x) >= size) {
                    bundles.inflate(y,x,y,y,x,x,emb);
                    return true;
                }
    size_t minw = (size + shore - 1)/shore;
    size_t maxw = min(cells.topo.dim[0], cells.topo.dim[1]);
    if (max_length > 0) maxw = min(max_length - 1, maxw);
    for(size_t width = minw; width <= maxw; width++) {
        clique_cache<chimera_spec> rects(cells, bundles, width);
        clique_iterator<chimera_spec> iter(cells, rects);
        if(iter.next(emb)) {
            if(emb.size() < size)
                emb.clear();
            else {
                max_length = width + 1;
                return true;
            }
        }
    }
    return false;
}

template<>
bool find_clique_nice(const cell_cache<pegasus_spec> &cells,
                      size_t size,
                      vector<vector<size_t>> &emb,
                      size_t &max_length) {
    bundle_cache<pegasus_spec> bundles(cells);
    size_t minw = (size + 1)/2;
    size_t maxw = cells.topo.dim[0];
    if(max_length == 0) {
        //naive first-pass: search for the first embedding with any max chainlength
        for(; minw <= maxw; minw++) {
            vector<vector<size_t>> tmp;
            clique_cache<pegasus_spec> rects(cells, bundles, minw);
            if(rects.extract_solution(tmp))
                //if we find an embedding, check that it's big enough
                if(tmp.size() >= size) {
                    emb = tmp;
                    max_length = get_maxlen(tmp, size);
                    maxw = min(maxw, minw + 6);
                    break;
                }
        }
        if(minw > maxw) return false;
    } else {
        maxw = (max_length)*6;
    }
    //we've already found an embedding; now try to find one with shorter chains
    for(size_t w = minw; w <= maxw; w++) {
        auto check_length = [&bundles, max_length](size_t yc, size_t xc,
                                                   size_t y0, size_t y1,
                                                   size_t x0, size_t x1){
            return bundles.length(yc,xc,y0,y1,x0,x1) < max_length; 
        };
        clique_cache<pegasus_spec> rects(cells, bundles, w, check_length);
        vector<vector<size_t>> tmp;
        if(rects.extract_solution(tmp)) {
            //if we find an embedding, check that it's big enough
            if(tmp.size() >= size) {
                //if it's big enough, find the max (necessary) chainlength
                size_t tlen = get_maxlen(tmp, size);
                if(tlen < max_length) {
                    emb = tmp;
                    max_length = tlen;
                    maxw = min(maxw, w+6);
                    w--; //re-do this width until we stop improving
                }
            }
        } //if no embeddings were found, keep searching
    }
    return emb.size() >= size;
}

template<typename topo_spec>
bool find_clique(const topo_spec &,
                 const vector<size_t> &nodes,
                 const vector<pair<size_t, size_t>> &edges,
                 size_t size,
                 vector<vector<size_t>> &emb);

template<>
bool find_clique(const chimera_spec & topo,
                 const vector<size_t> &nodes,
                 const vector<pair<size_t, size_t>> &edges,
                 size_t size,
                 vector<vector<size_t>> &emb) {
    switch(size) {
      case 0: return true;
      case 1: return find_generic_1(nodes, emb);
      case 2: return find_generic_2(edges, emb);
      default: break;
    }
    topo_cache<chimera_spec> chimera(topo, nodes, edges);
    size_t maxlen = 0;
    vector<vector<size_t>> _emb;
    if (find_clique_nice(chimera.cells, size, _emb, maxlen))
        emb = _emb;
    while(chimera.next())
        if (find_clique_nice(chimera.cells, size, _emb, maxlen))
            emb = _emb;
    return emb.size() >= size;
}

template<>
bool find_clique(const pegasus_spec &topo,
                 const vector<size_t> &nodes,
                 const vector<pair<size_t, size_t>> &edges,
                 size_t size,
                 vector<vector<size_t>> &emb) {
    switch(size) {
      case 0: return true;
      case 1: return find_generic_1(nodes, emb);
      case 2: return find_generic_2(edges, emb);
      case 3: if(find_generic_3(edges, emb)) return true; else break;
      case 4: if(find_generic_4(edges, emb)) return true; else break;
      default: break;
    }
    topo_cache<pegasus_spec> pegasus(topo, nodes, edges);
    size_t maxlen = 0;
    vector<vector<size_t>> _emb;
    if (find_clique_nice(pegasus.cells, size, _emb, maxlen))
        emb = _emb;
    while(pegasus.next()) {
        if (find_clique_nice(pegasus.cells, size, _emb, maxlen))
            emb = _emb;
    }
    return emb.size() >= size;
}

template<typename topo_spec>
bool find_clique_nice(const topo_spec &topo,
                      const vector<size_t> &nodes,
                      const vector<pair<size_t, size_t>> &edges,
                      size_t size,
                      vector<vector<size_t>> &emb) {
    const cell_cache<topo_spec> cells(topo, nodes, edges);
    size_t _;
    return find_clique_nice(cells, size, emb, _);
}

}
