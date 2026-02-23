// Copyright 2026 Christian Granzin
// Copyright 2008 Christophe Henry
// henry UNDERSCORE christophe AT hotmail DOT com
// This is an extended version of the state machine available in the boost::mpl library
// Distributed under the same license as the original.
// Copyright for the original version:
// Copyright 2005 David Abrahams and Aleksey Gurtovoy. Distributed
// under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#ifndef BOOST_MSM_BACKMP11_DETAIL_ANY_EVENT_HPP
#define BOOST_MSM_BACKMP11_DETAIL_ANY_EVENT_HPP

#include <boost/msm/backmp11/detail/basic_polymorphic.hpp>
#include <typeinfo>

namespace boost::msm::backmp11::detail
{

class any_event : public basic_polymorphic_base<>
{
    using base = basic_polymorphic_base<>;

  public:
    template <typename U>
    any_event(const U& obj) : base(obj), m_hash_code(typeid(U).hash_code())
    {
    }

    size_t hash_code() const
    {
        return m_hash_code;
    }

  private:
    size_t m_hash_code{0};
};

} // namespace boost::msm::backmp11::detail

#endif // BOOST_MSM_BACKMP11_DETAIL_ANY_EVENT_HPP
