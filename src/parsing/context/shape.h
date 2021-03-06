#ifndef SVG_CONVERTER_PARSING_CONTEXT_SHAPE_H_
#define SVG_CONVERTER_PARSING_CONTEXT_SHAPE_H_

#include <utility>
#include <vector>

#include <boost/mpl/set.hpp>

#include "../../math_defs.h"
#include "../dashes.h"
#include "../path.h"
#include "../svgpp.h"
#include "../traversal.h"
#include "../viewport.h"
#include "base.h"

namespace detail {

/**
 * Warn about a type of paint server being unsupported for an attribute.
 */
template <class AttributeTag, class... Args>
void warn_unsupported_paint_server(spdlog::logger& logger,
                                   AttributeTag /*unused*/,
                                   Args... /*unused*/) {
    logger.warn("Unsupported value type for attribute {}",
                svgpp::attribute_name<char>::get<AttributeTag>());
}

// Overload to only debug log warnings about simple colors, because they are
// sometimes necessary (to make an element clickable, or to make it viewable
// for debugging).
template <class AttributeTag>
void warn_unsupported_paint_server(spdlog::logger& logger,
                                   AttributeTag /*unused*/,
                                   int /*unused color*/,
                                   svgpp::tag::skip_icc_color /*unused*/ = {}) {
    logger.debug("Ignoring color value for attribute {}",
                 svgpp::attribute_name<char>::get<AttributeTag>());
}

}  // namespace detail

/**
 * Context for shape elements, like <path> or <rect>.
 *
 * SVG++ automatically converts all shapes to paths and then to a minimal subset
 * of the path commands, so that we only need to implement a few methods.
 */
template <class Exporter>
class ShapeContext : public BaseContext<Exporter> {
 private:
    /**
     * Saved shape path.
     */
    Path path_;

    /**
     * Describes the pattern of the stroke, set by `stroke-dasharray`.
     */
    std::vector<double> dasharray_;

    /**
     * IRI specified with the `fill` attribute.
     *
     * Empty if the element should not be filled. Even though the SVG standard
     * defaults `fill` to `black`, we default to not filling an element because
     * there is no good default fill.
     */
    std::string fill_fragment_iri_;

    /**
     * Whether the stroke should be plotted.
     */
    bool stroke_ = true;

 public:
    template <class ParentContext>
    explicit ShapeContext(ParentContext& parent);

    /**
     * Outline path of the shape.
     *
     * Used by `PatternContext` to fill this shape with a pattern.
     */
    const Path& outline_path() const { return path_; }

    /**
     * Used by `BaseContext` to select the viewport for child elements.
     */
    const Viewport& inner_viewport() const { return this->viewport(); }

    /**
     * Used by `BaseContext` to select the exporter for child elements.
     */
    Exporter inner_exporter() const { return this->exporter_; }

    /**
     * Whether child elements should be processed.
     */
    bool process_children() const { return true; }

    /**
     * SVG++ event for a non drawn movement in a shape path.
     */
    void path_move_to(double x, double y,
                      svgpp::tag::coordinate::absolute /*unused*/) {
        path_.push_command(MoveCommand{{x, y}});
    }

    /**
     * SVG++ event for a straight line in a shape path.
     */
    void path_line_to(double x, double y,
                      svgpp::tag::coordinate::absolute /*unused*/) {
        path_.push_command(LineCommand{{x, y}});
    }

    /**
     * SVG++ event for a cubic bezier part of a shape path.
     */
    void path_cubic_bezier_to(double x1, double y1, double x2, double y2,
                              double x, double y,
                              svgpp::tag::coordinate::absolute /*unused*/) {
        path_.push_command(BezierCommand{{x, y}, {x1, y1}, {x2, y2}});
    }

    /**
     * SVG++ event for a straight line to the start of the current subpath.
     */
    void path_close_subpath() { path_.push_command(CloseSubpathCommand{}); }

    /**
     * SVG++ event after the last shape command.
     */
    void path_exit() {}

    /**
     * SVG++ event fired when the element has been fully processed.
     */
    void on_exit_element();

    /**
     * SVG++ event when `stroke-dasharray` is set to an emtpy value.
     */
    void set(svgpp::tag::attribute::stroke_dasharray /*unused*/,
             svgpp::tag::value::none /*unused*/) {
        dasharray_.clear();
    }

    /**
     * SVG++ event when `stroke-dasharray` is set to a non empty value.
     */
    template <class Range>
    void set(svgpp::tag::attribute::stroke_dasharray /*unused*/,
             const Range& range) {
        dasharray_.assign(boost::begin(range), boost::end(range));
    }

    template <class... Args>
    void set(svgpp::tag::attribute::stroke tag, Args... args) {
        detail::warn_unsupported_paint_server(this->logger(), tag, args...);
    }

    void set(svgpp::tag::attribute::stroke /*unused*/,
             svgpp::tag::value::none /*unused*/) {
        stroke_ = false;
    }

    template <class... Args>
    void set(svgpp::tag::attribute::fill tag, Args... args) {
        detail::warn_unsupported_paint_server(this->logger(), tag, args...);
    }

    void set(svgpp::tag::attribute::fill /*unused*/,
             svgpp::tag::value::none /*unused*/) {
        fill_fragment_iri_.clear();
    }

    template <class String>
    void set(svgpp::tag::attribute::fill /*unused*/,
             svgpp::tag::iri_fragment /*unused*/, const String& id) {
        fill_fragment_iri_.assign(boost::begin(id), boost::end(id));
    }
};

template <class Exporter>
template <class ParentContext>
ShapeContext<Exporter>::ShapeContext(ParentContext& parent)
    : BaseContext<Exporter>{parent} {}

template <class Exporter>
void ShapeContext<Exporter>::on_exit_element() {
    using ExpectedElements = boost::mpl::set1<svgpp::tag::element::pattern>;
    // Override the processed elements setting just for the referenced
    // element. Allows us to process <pattern> only when referenced.
    using ProcessedElements = ExpectedElements;

    path_.transform(this->to_root());

    if (!fill_fragment_iri_.empty()) {
        auto referenced_node = this->document().find_by_id(fill_fragment_iri_);
        DocumentTraversal::load_referenced_element<
            svgpp::expected_elements<ExpectedElements>,
            svgpp::processed_elements<ProcessedElements>>::load(referenced_node,
                                                                *this);
    }

    if (stroke_) {
        // We move the path and dasharray, so that an exporter can store
        // them for later use without copying.
        DashedPath dashed_path{
            std::move(path_), std::move(dasharray_),
            this->to_root().inverse(Eigen::TransformTraits::AffineCompact)};
        this->exporter_.plot(std::move(dashed_path));
    }
}

#endif  // SVG_CONVERTER_PARSING_CONTEXT_SHAPE_H_
