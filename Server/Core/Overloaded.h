#ifndef OVERLOADED_H
#define OVERLOADED_H

#include <concepts>
#include <type_traits>

// Combines lambdas into one visitor for std::visit; each alternative picks its best match.
template<class... Visitors>
struct Overloaded : Visitors... { using Visitors::operator()...; };
template<class... Visitors>
Overloaded(Visitors...) -> Overloaded<Visitors...>;

// Matches any of the listed types, so one visitor lambda can handle a group of alternatives.
template<class T, class... Types>
concept OneOf = (std::same_as<std::remove_cvref_t<T>, Types> || ...);

#endif //OVERLOADED_H
