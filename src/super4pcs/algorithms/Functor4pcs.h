//
// Created by Sandra Alfaro on 24/04/18.
//

#ifndef SUPER4PCS_FUNCTOR4PCS_H
#define SUPER4PCS_FUNCTOR4PCS_H

#include <vector>
#include "super4pcs/shared.h"
#include "../accelerators/kdtree.h"
#include "../accelerators/utils.h"
#include "FunctorFeaturePointTest.h"

#include <fstream>
#include <time.h>  //clock


namespace gr {
    template <typename PointFilterFunctor = FilterTests>
    struct Match4PCS {
    public :
        using TypeBase = std::vector<Point3D>;
        using Scalar      = typename Point3D::Scalar;
        using PairsVector = std::vector< std::pair<int, int> >;
        using VectorType  = typename Point3D::VectorType;
        using OptionType  = Match4PCSOptions;


    private :
        OptionType myOptions_;
        std::vector<Point3D>& mySampled_Q_3D_;
        TypeBase &myBase_3D_;


    public :
        inline Match4PCS(std::vector<Point3D> &sampled_Q_3D_,
                         TypeBase& base_3D_,
                         OptionType options)
                        :mySampled_Q_3D_(sampled_Q_3D_)
                        ,myBase_3D_(base_3D_)
                        ,myOptions_ (options) {}

        // Initialize all internal data structures and data members.
        inline void Initialize(const std::vector<Point3D>& /*P*/,
                                   const std::vector<Point3D>& /*Q*/) {}

        // Finds congruent candidates in the set Q, given the invariants and threshold distances.
        inline bool FindCongruentQuadrilaterals(
                                         Scalar invariant1,
                                         Scalar invariant2,
                                         Scalar /*distance_threshold1*/,
                                         Scalar distance_threshold2,
                                         const std::vector <std::pair<int, int>> &First_pairs,
                                         const std::vector <std::pair<int, int>> &Second_pairs,
                                         Traits4pcs::Set* quadrilaterals) const {
            using RangeQuery = typename gr::KdTree<Scalar>::template RangeQuery<>;

            if (quadrilaterals == nullptr) return false;

            size_t number_of_points = 2 * First_pairs.size();

            // We need a temporary kdtree to store the new points corresponding to
            // invariants in the First_pairs and then query them (for range search) for all
            // the new points corresponding to the invariants in Second_pairs.
            quadrilaterals->clear();

            gr::KdTree<Scalar> kdtree(number_of_points);

            // Build the kdtree tree using the invariants on First_pairs.
            for (size_t i = 0; i < First_pairs.size(); ++i) {
                const VectorType &p1 = mySampled_Q_3D_[First_pairs[i].first].pos();
                const VectorType &p2 = mySampled_Q_3D_[First_pairs[i].second].pos();
                kdtree.add(p1 + invariant1 * (p2 - p1));
            }
            kdtree.finalize();

            //Point3D invRes;
            // Query the Kdtree for all the points corresponding to the invariants in Second_pairs.
            for (size_t i = 0; i < Second_pairs.size(); ++i) {
                const VectorType &p1 = mySampled_Q_3D_[Second_pairs[i].first].pos();
                const VectorType &p2 = mySampled_Q_3D_[Second_pairs[i].second].pos();

                RangeQuery query;
                query.queryPoint = p1 + invariant2 * (p2 - p1);
                query.sqdist = distance_threshold2;

                kdtree.doQueryDistProcessIndices(query,
                                                 [quadrilaterals, i, &First_pairs, &Second_pairs](int id) {
                                                     quadrilaterals->push_back(
                                                             { First_pairs[id / 2].first,
                                                               First_pairs[id / 2].second,
                                                               Second_pairs[i].first,
                                                               Second_pairs[i].second });
                                                 });
            }

            return quadrilaterals->size() != 0;
        }


       inline void ExtractPairs(Scalar pair_distance,
                                     Scalar pair_normals_angle,
                                     Scalar pair_distance_epsilon,
                                     int base_point1,
                                     int base_point2,
                                     PairsVector* pairs) const {
            if (pairs == nullptr) return;

            pairs->clear();
            pairs->reserve(2 * mySampled_Q_3D_.size());

            VectorType segment1 = (myBase_3D_[base_point2].pos() -
                    myBase_3D_[base_point1].pos()).normalized();


            // Go over all ordered pairs in Q.
            for (size_t j = 0; j < mySampled_Q_3D_.size(); ++j) {
                const Point3D& p = mySampled_Q_3D_[j];
                for (size_t i = j + 1; i < mySampled_Q_3D_.size(); ++i) {
                    const Point3D& q = mySampled_Q_3D_[i];
                    // Compute the distance and two normal angles to ensure working with
                    // wrong orientation. We want to verify that the angle between the
                    // normals is close to the angle between normals in the base. This can be
                    // checked independent of the full rotation angles which are not yet
                    // defined by segment matching alone..
                    const Scalar distance = (q.pos() - p.pos()).norm();
#ifndef MULTISCALE
                    if (std::abs(distance - pair_distance) > pair_distance_epsilon) continue;
#endif

                    PointFilterFunctor fun(myOptions_, myBase_3D_);
                    std::pair<bool,bool> res = fun(p,q, pair_normals_angle, base_point1,base_point2);
                    if (res.first)
                        pairs->emplace_back(i, j);
                    if (res.second)
                        pairs->emplace_back(j, i);
                }
            }
        }

     };
}


#endif //SUPER4PCS_FUNCTOR4PCS_H