typedef struct _volbox {
	GtkWidget *volbox;
	GtkWidget *volbarbox;
	GtkWidget *vollabel;
	GtkWidget *volbar;
	GtkWidget *rvolbar;
	GtkWidget *muteswitch;
	GtkWidget *lockswitch;
	snd_mixer_elem_t   *volelem;
	glong minrange;
	glong maxrange;
	glong curval;
	glong rcurval;
	gulong hsignalid;
	gulong hsignalidr;
	gulong htogsignalid;
	gulong hmutesignalid;
	guint type;
} VolBox;

typedef struct _switchbox {
	GtkWidget *volswitch;
	snd_mixer_elem_t   *volelem;
	gulong hswsignalid;
	guint type;
} SwitchBox;

typedef struct _enumbox {
	GtkWidget *enumhbox;
	GtkWidget *enumname;
	GtkWidget *enumswitch;
	snd_mixer_elem_t   *volelem;
	gulong hswsignalid;
	guint type;
} EnumBox;

typedef struct _mixerelem {
	VolBox *playback;
	VolBox *capture;
	SwitchBox *pswitch;
	SwitchBox *cswitch;
	EnumBox *penum;
	EnumBox *cenum;
	snd_mixer_elem_t *elem;
}MixerElem;

GList* SoundCards = NULL;
GList* VolumeBoxes = NULL;

static int n_alsa_chans = 0;
static GIOChannel **alsa_chans = NULL;

static snd_mixer_t *mixer_handle;
static snd_mixer_selem_id_t *sid;

static int lamixer_volbox_change(snd_mixer_elem_t *elem, unsigned int mask);
static int lamixer_enumbox_change(snd_mixer_elem_t *elem, unsigned int mask);
static int lamixer_switchbox_change(snd_mixer_elem_t *elem, unsigned int mask);

void lamixer_enumbox_changed(GtkObject *enumswitch, snd_mixer_elem_t *elem);
void lamixer_volswitch_changed(GtkObject *checkbutton, snd_mixer_elem_t *elem);
void lamixer_muteswitch_changed(GtkObject *checkbutton, snd_mixer_elem_t *elem);
void lamixer_volbox_changed(GtkObject *adjustment, VolBox *volumebox);
void lamixer_volboxr_changed(GtkObject *adjustment, VolBox *volumebox);
void lamixer_volbox_changevalue(GtkWidget *volbar, glong value, gulong hsignalid);
void lamixer_volbox_check(GtkWidget *volbar, gulong hsignalid, glong value, glong *svalue);
void lamixer_switch_checkvalue(GtkWidget *muteswitch, int mutesw, gulong hmutesignalid);

static gboolean lamixer_volbox_alsa_io(GIOChannel *chan, GIOCondition condition, gpointer data);

void lamixer_switchbox_show(SwitchBox *swbox, GtkWidget *swvbox);
void lamixer_enumbox_show(EnumBox *enumbox, GtkWidget *enumvbox);
void lamixer_volbox_show(VolBox *volumebox, snd_mixer_elem_t *elem, GtkWidget *mixerbox);

void lamixer_mixerelem_show(MixerElem *mixer_elem);

void lamixer_enum_cards();
void lamixer_mixer_init(char card[64]);
void lamixer_mixer_elemets();
void lamixer_mixer_switch();

void lamixer_widgets_destroy(GtkWidget *container);
void lamixer_mixer_destroy();

void lamixer_window_resize();

void InitMixer(char card[64]);
void InitScreen(void);
void InitMenu();

MixerElem *lamixer_volelem_new(snd_mixer_elem_t *elem);
EnumBox *lamixer_enumbox_add(const gchar *str, snd_mixer_elem_t *elem, guint voltype);
SwitchBox *lamixer_switchbox_add(const gchar *str, snd_mixer_elem_t *elem, guint voltype);
VolBox *lamixer_volbox_new(const gchar *str, const gint mono, const gint mute, const glong minrange, const glong maxrange, snd_mixer_elem_t *elem, guint voltype);
