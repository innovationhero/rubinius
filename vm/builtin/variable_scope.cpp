#include "vm.hpp"
#include "objectmemory.hpp"
#include "call_frame.hpp"
#include "gc/gc.hpp"

#include "builtin/object.hpp"
#include "builtin/variable_scope.hpp"
#include "builtin/class.hpp"
#include "builtin/system.hpp"
#include "builtin/tuple.hpp"

#include "builtin/fiber.hpp"
#include "fiber_data.hpp"

#include "ontology.hpp"

namespace rubinius {
  void VariableScope::init(STATE) {
    GO(variable_scope).set(ontology::new_class(state,
          "VariableScope", G(object), G(rubinius)));
    G(variable_scope)->set_object_type(state, VariableScopeType);
  }

  void VariableScope::bootstrap_methods(STATE) {
    GCTokenImpl gct;
    System::attach_primitive(state, gct,
                             G(variable_scope), false,
                             state->symbol("method_visibility"),
                             state->symbol("variable_scope_method_visibility"));
  }

  VariableScope* VariableScope::of_sender(STATE, CallFrame* call_frame) {
    CallFrame* dest = call_frame->previous;
    // Skip any frames for native methods
    while(dest->native_method_p()) { dest = dest->previous; }
    return dest->promote_scope(state);
  }

  VariableScope* VariableScope::current(STATE, CallFrame* call_frame) {
    if(call_frame->native_method_p()) return nil<VariableScope>();
    return call_frame->promote_scope(state);
  }

  VariableScope* VariableScope::synthesize(STATE, CompiledCode* method,
                                           Module* module, Object* parent,
                                           Object* self, Object* block,
                                           Tuple* locals)
  {
    VariableScope* scope = state->new_object<VariableScope>(G(variable_scope));

    scope->block(state, block);
    scope->module(state, module);
    scope->method(state, method);

    if(VariableScope* vs = try_as<VariableScope>(parent)) {
      scope->parent(state, vs);
    } else {
      scope->parent(state, nil<VariableScope>());
    }

    scope->heap_locals(state, locals);
    scope->last_match(state, cNil);

    scope->self(state, self);
    scope->number_of_locals_ = locals->num_fields();
    scope->isolated_ = true;
    scope->locals_ = 0;
    scope->block_as_method_ = 0;

    return scope;
  }

  Tuple* VariableScope::locals(STATE) {
    Tuple* tup = Tuple::create(state, number_of_locals_);
    for(int i = 0; i < number_of_locals_; i++) {
      tup->put(state, i, get_local(state, i));
    }

    return tup;
  }

  Object* VariableScope::set_local_prim(STATE, Fixnum* number, Object* object) {
    int num = number->to_int();

    if(num < 0) {
      Exception::argument_error(state, "negative local index");
    } else if(num >= number_of_locals_) {
      Exception::argument_error(state, "index larger than number of locals");
    }

    set_local(state, num, object);
    return cNil;
  }

  // bootstrap method, replaced with an attr_accessor in kernel.
  Object* VariableScope::method_visibility(STATE) {
    return cNil;
  }

  void VariableScope::set_local(STATE, int pos, Object* val) {
    if(isolated_) {
      while(heap_locals_->nil_p()) {
        atomic::pause();
      }
      heap_locals_->put(state, pos, val);
    } else {
      set_local(pos, val);
    }
  }

  void VariableScope::set_local(int pos, Object* val) {
    Object** ary = locals_;

    if(Fiber* fib = try_as<Fiber>(fiber_)) {
      FiberData* data = fib->data();
      if(data) {
        AddressDisplacement dis(data->data_offset(),
                                data->data_lower_bound(),
                                data->data_upper_bound());

        ary = dis.displace(ary);
      }
    }
    ary[pos] = val;
  }

  Object* VariableScope::get_local(STATE, int pos) {
    if(isolated_) {
      while(heap_locals_->nil_p()) {
        atomic::pause();
      }
      return heap_locals_->at(pos);
    } else {
      return get_local(pos);
    }
  }

  Object* VariableScope::get_local(int pos) {
    Object** ary = locals_;
    if(Fiber* fib = try_as<Fiber>(fiber_)) {
      FiberData* data = fib->data();
      if(data) {
        AddressDisplacement dis(data->data_offset(),
                                data->data_lower_bound(),
                                data->data_upper_bound());

        ary = dis.displace(ary);
      }
    }
    return ary[pos];
  }

  void VariableScope::flush_to_heap(STATE) {
    if(isolated_) return;
    if(!atomic::compare_and_swap(&isolated_, 0, 1)) return;

    Tuple* new_locals = Tuple::create(state, number_of_locals_);
    for(int i = 0; i < number_of_locals_; i++) {
      new_locals->put(state, i, locals_[i]);
    }
    atomic::memory_barrier();
    heap_locals(state, new_locals);
  }

  void VariableScope::Info::mark(Object* obj, ObjectMark& mark) {
    auto_mark(obj, mark);

    VariableScope* vs = as<VariableScope>(obj);

    vs->fixup();

    if(!vs->isolated()) {
      Object** ary = vs->locals_;

      if(Fiber* fib = try_as<Fiber>(vs->fiber())) {
        FiberData* data = fib->data();

        if(data) {
          AddressDisplacement dis(data->data_offset(),
                                  data->data_lower_bound(),
                                  data->data_upper_bound());

          ary = dis.displace(ary);
        }
      }

      size_t locals = vs->number_of_locals();

      for(size_t i = 0; i < locals; i++) {
        Object* tmp = mark.call(ary[i]);
        if(tmp) { ary[i] = tmp; }
      }
    }
  }
}
