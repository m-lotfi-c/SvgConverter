#include "conversion.h"

#include <svgpp/policy/xml/libxml2.hpp>
#include <svgpp/svgpp.hpp>

#include "conversion/context/base.h"
#include "conversion/context/factories.h"
#include "utility.h"


/**
 * List of elements which should be processed.
 */
using processed_elements_t = Concat<
        // Elements describing shapes
        svgpp::traits::shape_elements,

        // Supported structural elements
        boost::mpl::set<
            svgpp::tag::element::svg,
            svgpp::tag::element::g
        >
    >;

/**
 * List of attributes which should be processed.
 */
using processed_attributes_t = Concat<
        // Attributes describing the shape of shape elements
        svgpp::traits::shapes_attributes_by_element,

        // Other attributes
        boost::mpl::set<  // NOLINT not the stl set (no #include <set> needed)
            svgpp::tag::attribute::transform
        >
    >;

/**
 * Policy on how to handle paths (and other elements converted to paths).
 *
 * The `minimal` policy does all the conversions described at
 * http://svgpp.org/path.html#path-policy-concept. The conversion from arcs
 * to bézier curves is lossy. Because the silhouette can only plot arcs of
 * circles but not arcs a conversion would be necessary anyway. Can be
 * replaced with another conversion if artifacts start to appear.
 */
using path_policy_t = svgpp::policy::path::minimal;

std::string convert(const ManagedXmlDoc& svgDoc) {
    BaseContext context;
    svgpp::document_traversal<
            svgpp::processed_elements<processed_elements_t>,
            svgpp::processed_attributes<processed_attributes_t>,
            svgpp::context_factories<ContextFactories>,
            svgpp::path_policy<path_policy_t>
    >::load_document(getRoot(svgDoc), context);
    return {};
}
