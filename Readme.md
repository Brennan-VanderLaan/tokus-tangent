# Tokus Tangent

### What?
So I'm working on a few ideas for VCV Rack and this is where I'll be gathering them.



### VCV Radio (WIP)

Not sure how far this will go, but the idea is to set up a simple peer-to-peer audio streaming module


Listeners can pull streams.
Broadcasters can push streams out to be digested.
Servers facilitate connection hand offs.

### Modules that are stable

---

<*crickets*>


<br><br>
### Modules that are past the dreaming stage

---

#### ./Client (2 Channel Client)
- Lets you connect to a remote server module
- proof of concept for radio

#### ./Server (2 Channel Server)
- Lets you accept a connection from a remote client
- proof of concept for radio

<br><br>
### Modules that are solidly in the dreaming stage~

---

#### RadioReturn (BROADCASTER EXTENSION)
- Lets a radio client return a stream of data as a 'response' stream to the broadcaster

#### RadioBroadcast (BROADCASTER)
- Lets you push a stream out remotely
- Broadcast handle
- \# channels
- bit perfect stream
- bit rate stream

#### RadioDelay (RadioClient Extension)
- A dynamic buffer that will delay your local signals based on the current connection information


#### WebClient (LISTENER)
- Lets you pull a stereo stream via web client (how? do we need a 'relay' type of node?)

#### Server
- Tracks live sessions and serves as a central command and control point for the radio
- List handles / streams associated with that handle

#### What's going to go wrong?

One of the anticipated issues with this project is latency / variable latency
(There's also a throughput / bandwidth rabbit hole, but I'm not thinking about that yet.)

At the very least off the bat we need to be able to track the latency between peers, 
that is going to tie into the buffer size for the radio modules. I also want to explore 
the possibility of an I/O module that lets you both send and receive from another I/O 
module - this is considerably more complex. I imagine the biggest issues will be around
maintaining what I'm calling 'musical sync', but is centered around beat matching.

One of the main goals of these modules is to enable collaboration between artists using
VCV Rack as their medium to produce sounds, what I imagine happening is someone produces
a stream of CV (say the main clock and a set of pitches) and then any number of listeners
can implement the VCO / VCF chain and kick back audio based on that stream as a "response stream"

The broadcaster would be able to access those response streams as a set of inputs back
into VCV Rack.

#### How?

The plan is to use TCP as the initial transport and focus on goals in this order

- Static buffer size, implement RadioClient & RadioBroadcast to use address typed in (no auto-discovery)
- Expose buffer status to UI
- Add latency tracking
- Add buffer tracking
- Add buffer cv controls
- Add central server
- Change transport layer out if needed
- Broadcaster tags
- Broadcast response stream
- Webclient
- TBD?

#### Transport layer? What?

TCP is great for making sure it got there, but similar to the issues that games face
it does not do well with real time transmissions if if you have packet loss / variable
latency (it will still get there, you just don't know how long it will take, and window
sizes can move quickly)

So what?

[https://improbable.io/blog/kcp-a-new-low-latency-secure-network-stack]

Enter KCP. TCP redesigned with a focus on low latency, packet reconstruction, and wasting bandwidth
in exchange for faster transmission times. My plan is to use KCP over UDP to achieve a lower latency
but firewall friendly application layer. Still need to research bandwidth requirements for this, as
raw wav data potentially will use huge amounts of bandwidth regardless of the transport layer.

It looks like this is a simple callback hook system so that should play nicely with the module setup.

On the webclient I want to find out if multicast is the right approach to sending the same stream to many people,
although this may not be an issue at all considering I don't expect heavy adoption.

#### Compression

We probably need to compress the audio data. That needs research / can be implemented when
this moves towards living in the web instead of as a local project. It also may be neat to offer the ability
to use common compression algorithms intentionally as that may yield interesting effects on the sound.

Need to look into what the licenses are around those, if they are open that could be a lot of fun.



### VCV Game (TBD)

What if you could build a patch that is your game's "sound engine".