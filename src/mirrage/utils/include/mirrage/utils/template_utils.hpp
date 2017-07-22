/** small helpers for template programming ***********************************
 *                                                                           *
 * Copyright (c) 2014 Florian Oetke                                          *
 *  This file is distributed under the MIT License                           *
 *  See LICENSE file for details.                                            *
\*****************************************************************************/

#pragma once

#include <mirrage/utils/log.hpp>

#include <vector>
#include <algorithm>
#include <functional>

namespace mirrage {
namespace util {

	template<typename T>
	struct void_t {};

	struct no_move {
		protected:
			no_move() = default;
			~no_move()noexcept = default;
			no_move(no_move&&)=delete;
			no_move& operator=(no_move&&)=delete;
	};
	struct no_copy {
		protected:
			no_copy() = default;
			~no_copy()noexcept = default;
			no_copy(const no_copy&)=delete;
			no_copy& operator=(const no_copy&)=delete;
	};
	struct no_copy_move : no_copy, no_move {
		protected:
			no_copy_move() = default;
			~no_copy_move()noexcept = default;
	};

	template<class Func>
	struct cleanup {
		cleanup(Func f)noexcept : active(true), f(f) {}
		cleanup(const cleanup&)=delete;
		cleanup(cleanup&& o)noexcept : active(o.active), f(o.f) {o.active=false;}
		~cleanup()noexcept{f();}

		bool active;
		Func f;
	};
	template<class Func>
	inline auto cleanup_later(Func&& f)noexcept {
		return cleanup<Func>{std::forward<Func>(f)};
	}

	namespace detail {
		enum class cleanup_scope_guard {};
		template<class Func>
		auto operator+(cleanup_scope_guard, Func&& f) {
			return cleanup<Func>{std::forward<Func>(f)};
		}
	}

#define CLEANUP_CONCATENATE_DIRECT(s1, s2) s1##s2
#define CLEANUP_CONCATENATE(s1, s2) CLEANUP_CONCATENATE_DIRECT(s1, s2)
#define CLEANUP_ANONYMOUS_VARIABLE(str) CLEANUP_CONCATENATE(str, __LINE__)

#define ON_EXIT auto CLEANUP_ANONYMOUS_VARIABLE(_on_scope_exit) = ::mirrage::util::detail::cleanup_scope_guard() + [&]


	template<typename T>
	constexpr bool dependent_false() {
		return false;
	}

	template<typename T>
	T identity(T t) {
		return t;
	}

	template<typename T>
	void erase_fast(std::vector<T>& c, const T& v) {
		using std::swap;

		auto e = std::find(c.begin(), c.end(), v);
		if(e!=c.end()) {
			swap(*e, c.back());
			c.pop_back();
		}
	}
	template<typename T>
	void erase_fast_stable(std::vector<T>& c, const T& v) {
		auto ne = std::remove(c.begin(), c.end(), v);

		if(ne!=c.end()) {
			c.erase(ne, c.end());
		}
	}

	template< typename T, typename PredicateT>
	void erase_if(std::vector<T>& items, const PredicateT& predicate) {
		items.erase(std::remove_if(items.begin(), items.end(), predicate), items.end());
	}

	template< typename ContainerT, typename PredicateT >
	void erase_if(ContainerT& items, const PredicateT& predicate) {
		for( auto it = items.begin(); it != items.end(); ) {
			if( predicate(*it) ) it = items.erase(it);
			else ++it;
		}
	}

	template<typename T, typename F>
	auto map(std::vector<T>& c, F&& f) {
		auto result = std::vector<decltype(f(std::declval<T&>()))>();
		result.reserve(c.size());

		for(auto& e : c) {
			result.emplace_back(f(e));
		}

		return result;
	}

	template<typename... Ts>
	auto make_vector(Ts&&... values) {
		auto vec = std::vector<std::common_type_t<Ts...>>();
		vec.reserve(sizeof...(values));

		apply([&](auto&& value) {
			vec.emplace_back(std::forward<decltype(value)>(value));
		}, std::forward<Ts>(values)...);

		return vec;
	}

	namespace detail {
	    template<std::size_t N, std::size_t... I, class F>
	    auto build_array_impl(F&& factory, std::index_sequence<I...>) {
	        return std::array<std::common_type_t<decltype(factory(I))...>, N>{{factory(I)...}};
	    }
	}

	template<std::size_t N, class F>
	auto build_array(F&& factory) {
	    return detail::build_array_impl<N>(factory, std::make_index_sequence<N>());
	}

	namespace detail {
		using type_id_t = std::intptr_t;

		template<typename T>
		struct type { static void id() { } };

		template<typename T>
		type_id_t type_id() { return reinterpret_cast<type_id_t>(&type<T>::id); }
	}

	class any_ptr {
		public:
			any_ptr() : _ptr(nullptr), _type_id(0) {}
			any_ptr(std::nullptr_t) : _ptr(nullptr), _type_id(0) {}
			template<typename T>
			any_ptr(T* ptr) : _ptr(ptr), _type_id(detail::type_id<T>()) {}

			any_ptr& operator=(std::nullptr_t) {
				_ptr = nullptr;
				_type_id = 0;
				return *this;
			}
			template<typename T>
			any_ptr& operator=(T* ptr) {
				_ptr = ptr;
				_type_id = detail::type_id<T>();
				return *this;
			}

			template<typename T>
			auto try_extract()const -> T* {
				if(detail::type_id<T>()==_type_id)
					return static_cast<T*>(_ptr);
				else
					return nullptr;
			}

		private:
			void*             _ptr;
			detail::type_id_t _type_id;
	};

	template<class Iter>
	class iter_range {
		public:
			iter_range()noexcept {}
			iter_range(Iter begin, Iter end)noexcept : b(begin), e(end) {}

			bool operator==(const iter_range& o)noexcept {
				return b==o.b && e==o.e;
			}

			Iter begin()const noexcept {
				return b;
			}
			Iter end()const noexcept {
				return e;
			}

			std::size_t size()const noexcept {
				return std::distance(b,e);
			}

		private:
			Iter b, e;
	};
	template<class T>
	using vector_range = iter_range<typename std::vector<T>::iterator>;

	template<class T>
	using cvector_range = iter_range<typename std::vector<T>::const_iterator>;

	template<class T>
	class numeric_range {
		struct iterator : std::iterator<std::random_access_iterator_tag, T, T> {
			T p;
			T s;
			constexpr iterator(T v, T s=1)noexcept : p(v), s(s) {};
			constexpr iterator(const iterator&)noexcept = default;
			constexpr iterator(iterator&&)noexcept = default;
			iterator& operator++()noexcept {p+=s; return *this;}
			iterator operator++(int)noexcept {auto t=*this; *this++; return t;}
			iterator& operator--()noexcept {p-=s; return *this;}
			iterator operator--(int)noexcept {auto t=*this; *this--; return t;}
			bool operator==(const iterator& rhs)const noexcept {return p==rhs.p;}
			bool operator!=(const iterator& rhs)const noexcept {return p!=rhs.p;}
			const T& operator*()const noexcept {return p;}
		};
		using const_iterator = iterator;

		public:
			constexpr numeric_range()noexcept {}
			constexpr numeric_range(T begin, T end, T step=1)noexcept : b(begin), e(end), s(step) {}
			constexpr numeric_range(numeric_range&&)noexcept = default;
			constexpr numeric_range(const numeric_range&)noexcept = default;

			numeric_range& operator=(const numeric_range&)noexcept = default;
			numeric_range& operator=(numeric_range&&)noexcept = default;
			bool operator==(const numeric_range& o)noexcept {
				return b==o.b && e==o.e;
			}

			constexpr iterator begin()const noexcept {
				return b;
			}
			constexpr iterator end()const noexcept {
				return e;
			}

		private:
			T b, e, s;
	};
	template<class Iter, typename = std::enable_if_t<!std::is_arithmetic<Iter>::value> >
	constexpr iter_range<Iter> range(Iter b, Iter e) {
		return {b,e};
	}
	template<class B, class E, typename = std::enable_if_t<std::is_arithmetic<B>::value> >
	constexpr auto range(B b, E e, std::common_type_t<B,E> s=1) {
		using T = std::common_type_t<B,E>;
		return numeric_range<T>{T(b), std::max(T(e+1), T(b)), T(s)};
	}
	template<class T, typename = std::enable_if_t<std::is_arithmetic<T>::value> >
	constexpr numeric_range<T> range(T num) {
		return {0,static_cast<T>(num)};
	}
	template<class Container, typename = std::enable_if_t<!std::is_arithmetic<Container>::value> >
	auto range(Container& c) -> iter_range<typename Container::iterator> {
		using namespace std;
		return {begin(c),end(c)};
	}
	template<class Container, typename = std::enable_if_t<!std::is_arithmetic<Container>::value> >
	auto range(const Container& c) -> iter_range<typename Container::const_iterator> {
		using namespace std;
		return {begin(c),end(c)};
	}

	template<class Container, typename = std::enable_if_t<!std::is_arithmetic<Container>::value> >
	auto range_reverse(Container& c) -> iter_range<typename Container::reverse_iterator> {
		using namespace std;
		return {rbegin(c),rend(c)};
	}
	template<class Container, typename = std::enable_if_t<!std::is_arithmetic<Container>::value> >
	auto range_reverse(const Container& c) -> iter_range<typename Container::const_reverse_iterator> {
		using namespace std;
		return {rbegin(c),rend(c)};
	}


	template<class Iter, class Type>
	class cast_iterator {
		public:
			typedef typename Iter::iterator_category  iterator_category;
			typedef Type                              value_type;
			typedef std::ptrdiff_t                    difference_type;
			typedef Type*                             pointer;
			typedef Type&                             reference;


			cast_iterator(Iter iter) : iter(iter) {};

			reference operator*() {return *reinterpret_cast<pointer>(*iter);}
			pointer operator->() {return reinterpret_cast<pointer>(*iter);}

			cast_iterator& operator++() {
				++iter;
				return *this;
			}
			cast_iterator& operator--() {
				--iter;
				return *this;
			}

			cast_iterator operator++(int) {
				cast_iterator t = *this;
				++*this;
				return t;
			}

			cast_iterator operator--(int) {
				cast_iterator t = *this;
				--*this;
				return t;
			}

			bool operator==(const cast_iterator& o) const {
				return iter==o.iter;
			}
			bool operator!=(const cast_iterator& o) const {
				return iter!=o.iter;
			}
			bool operator<(const cast_iterator& o) const {
				return iter<o.iter;
			}


		private:
			Iter iter;
	};


	template<typename F>
	void doOnce(F f) {
		static bool first = true;
		if(first) {
			first = false;
			f();
		}
	}


	template<class CCTP, class ParentT>
	class Registered;

	template<class CCTP, class ChildT>
	class Registration {
		public:
			Registration() = default;
			Registration(Registration&& rhs)noexcept : _children(std::move(rhs._children)) {
				for(auto& c : _children) {
					Registered<ChildT,CCTP>::asRegistered(c)->_parent = this;
				}
			}
			Registration(const Registration& rhs) = delete;
			~Registration() {
				for(auto& c : _children) {
					Registered<ChildT,CCTP>::asRegistered(c)->_parent = nullptr;
				}
			}

			Registration& operator=(Registration&& rhs)noexcept {
				for(auto& c : _children) {
					Registered<ChildT,CCTP>::asRegistered(c)->_parent = nullptr;
				}

				_children = std::move(rhs._children);
				for(auto& c : _children) {
					Registered<ChildT,CCTP>::asRegistered(c)->_parent = this;
				}

				return *this;
			}
			Registration& operator=(const Registration& rhs) = delete;

		protected:
			auto children() noexcept -> auto& {
				return _children;
			}

		private:
			friend class Registered<ChildT,CCTP>;

			std::vector<ChildT*> _children;

			static Registration* asRegistration(CCTP* self) {
				return static_cast<Registration*>(self);
			}
	};

	template<class CCTP, class ParentT>
	class Registered {
		public:
			Registered()noexcept : _parent(nullptr) {}
			Registered(ParentT& p)noexcept : _parent(&p) {
				_registration()->_children.push_back(static_cast<CCTP*>(this));
			}
			Registered(Registered&& rhs)noexcept : _parent(rhs._parent) {
				rhs._parent = nullptr;

				if(_parent) {
					util::erase_fast(_registration()->_children, static_cast<CCTP*>(&rhs));
					_registration()->_children.push_back(static_cast<CCTP*>(this));
				}
			}
			Registered(const Registered& rhs)noexcept : _parent(rhs._parent) {
				if(_parent) {
					_registration()->_children.push_back(static_cast<CCTP*>(this));
				}
			}
			~Registered() {
				if(_parent) {
					// safe unless the casted this ptr is dereferenced
					util::erase_fast(_registration()->_children, static_cast<CCTP*>(this));
				}
			}

			Registered& operator=(Registered&& rhs)noexcept {
				if(_parent) {
					util::erase_fast(_registration()->_children, static_cast<CCTP*>(this));
				}

				_parent = rhs._parent;
				rhs._parent = nullptr;
				if(_parent) {
					util::erase_fast(_registration()->_children, static_cast<CCTP*>(&rhs));
					_registration()->_children.push_back(static_cast<CCTP*>(this));
				}

				return *this;
			}
			Registered& operator=(const Registered& rhs)noexcept {
				if(_parent) {
					util::erase_fast(_registration()->_children, static_cast<CCTP*>(this));
				}

				_parent = rhs._parent;
				if(_parent) {
					_registration()->_children.push_back(static_cast<CCTP*>(this));
				}

				return *this;
			}

		protected:
			auto parent()noexcept -> auto& {
				INVARIANT(_parent, "Deref nullptr");
				return *_parent;
			}

		private:
			friend class Registration<ParentT, CCTP>;
			ParentT* _parent;

			auto _registration() {
				return Registration<ParentT, CCTP>::asRegistration(_parent);
			}

			static Registered* asRegistered(CCTP* self) {
				return static_cast<Registered*>(self);
			}
	};

}
}

#define M_REPEAT_1(X) X(0)
#define M_REPEAT_2(X) M_REPEAT_1(X) X(1)
#define M_REPEAT_3(X) M_REPEAT_2(X) X(2)
#define M_REPEAT_4(X) M_REPEAT_3(X) X(3)
#define M_REPEAT_5(X) M_REPEAT_4(X) X(4)
#define M_REPEAT_6(X) M_REPEAT_5(X) X(5)
#define M_REPEAT_7(X) M_REPEAT_6(X) X(6)
#define M_REPEAT_8(X) M_REPEAT_7(X) X(7)
#define M_REPEAT_9(X) M_REPEAT_8(X) X(8)
#define M_REPEAT_10(X) M_REPEAT_9(X) X(9)
#define M_REPEAT_11(X) M_REPEAT_10(X) X(10)
#define M_REPEAT_12(X) M_REPEAT_11(X) X(11)
#define M_REPEAT_13(X) M_REPEAT_12(X) X(12)
#define M_REPEAT_14(X) M_REPEAT_13(X) X(13)
#define M_REPEAT_15(X) M_REPEAT_14(X) X(14)
#define M_REPEAT_16(X) M_REPEAT_15(X) X(15)
#define M_REPEAT_17(X) M_REPEAT_16(X) X(16)

#define M_EXPAND(...) __VA_ARGS__

#define M_REPEAT__(N, X) M_EXPAND(M_REPEAT_ ## N)(X)
#define M_REPEAT_(N, X) M_REPEAT__(N, X)
#define M_REPEAT(N, X) do{M_REPEAT_(M_EXPAND(N), X)}while(false)

