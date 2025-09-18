Module system uses subscriptions and messages.\
For example:

```c++
struct Message { ... };

struct IModule {
  // Send `Message` to subscription
  virtual void dispatchMessage(std::shared_ptr<const Message> message) = 0;

  // Received `Message` from subscription
  virtual void onMessage(std::shared_ptr<const Message> message) = 0;
};
struct Module : IModule {
  SimpleSubscription<Message> subscription_message_;

  void start() {
    subscription_message_.subscribe(*se_manager_, module_internal);
  }

  void dispatchMessage(std::shared_ptr<const Message> message) override {
    log("dispatch Message");
    dispatchDerive(subscription, message);
  }

  void onMessage(std::shared_ptr<const Message> message) override;
};
void Module::onMessage(std::shared_ptr<const Message> message) {
  log("received Message");
}
```

That code may look like boilerplate and duplicate message type name,\
but don't try to simplify it using macro.\
The following code is forbidden and will not pass PR review.

```c++
struct Message { ... };

struct IModule {
  // Send `Message` to subscription
  VIRTUAL_DISPATCH(Message);

  // Received `Message` from subscription
  VIRTUAL_ON_DISPATCH(Message);
};
struct Module : IModule {
  ON_DISPATCH_SUBSCRIPTION(Message);

  void start() {
    ON_DISPATCH_SUBSCRIBE(Message);
  }

  DISPATCH_OVERRIDE(Message) {
    log("dispatch Message");
    dispatchDerive(subscription, message);
  }

  ON_DISPATCH_OVERRIDE(Message);
};
ON_DISPATCH_IMPL(Module, Message) {
  log("received Message");
}
```

Don't try to preserve type name case in variable or member name.\
Yes, it breaks relation between type name and field name.\
Yes, you need to change case manually.\
The following code is forbidden and will not pass PR review.

```c++
SimpleSubscription<Message> subscription_Message_;
```

Don't try to separate type name in function name.\
The following code is forbidden and will not pass PR review.

```c++
void dispatch_Message(std::shared_ptr<const Message> message);
```
