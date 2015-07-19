#include <gtk/gtk.h>
#include <glib.h>
#include <alsa/asoundlib.h>
#include <math.h>
#include "lamixer.h"

GtkWidget *switchmixer;
GtkWidget *switchvbox;
GtkWidget *switchcapturevbox;
GtkWidget *enumvbox;
GtkWidget *enumcapturevbox;

GtkWidget *mixerbox;
GtkWidget *capturebox;
GtkWidget *playbackscrollwin;
GtkWidget *capturescrollwin;
GtkWidget *window;
GtkWidget *menu;
GtkWidget *windowbox;
GtkWidget *menu_bar;
GtkWidget *menu_mixer;
GtkWidget *menu_view;
GtkWidget *menu_help;

static char	 card_id[64] = "hw:0"; //FIXME: How can we detect default card?

static int pCols, cCols;
const static int mRows = 5;

void lamixer_muteswitch_changed(GtkObject *checkbutton, snd_mixer_elem_t *elem)
{
	snd_mixer_selem_set_playback_switch_all (elem, !gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(checkbutton)));
	//FIXME: Some mixer elemets may have separated mute-switch per channel
}

void lamixer_volswitch_changed(GtkObject *checkbutton, snd_mixer_elem_t *elem)
{
	snd_mixer_selem_set_playback_switch_all (elem, gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(checkbutton)));
	//FIXME: Some mixer elemets may have separated mute-switch per channel
}

static int lamixer_switchbox_change(snd_mixer_elem_t *elem, unsigned int mask)
{
	SwitchBox *swbox;
	int volsw;

	if (mask == SND_CTL_EVENT_MASK_REMOVE)
		return 0;

	if (mask & SND_CTL_EVENT_MASK_VALUE) {
		swbox = snd_mixer_elem_get_callback_private(elem);

		if (swbox->type == 1) {
			snd_mixer_selem_get_playback_switch (swbox->volelem, SND_MIXER_SCHN_FRONT_LEFT, &volsw);

			if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(swbox->volswitch)) != volsw) {
				g_signal_handler_block(G_OBJECT(swbox->volswitch), swbox->hswsignalid);
				gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(swbox->volswitch), volsw);
				g_signal_handler_unblock(G_OBJECT(swbox->volswitch), swbox->hswsignalid);
			}

		}
	}

	return 0;
}

void lamixer_switch_checkvalue(GtkWidget *muteswitch, int mutesw, gulong hmutesignalid)
{
	if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(muteswitch)) == mutesw) {
		g_signal_handler_block(G_OBJECT(muteswitch), hmutesignalid);
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(muteswitch), !mutesw);
		g_signal_handler_unblock(G_OBJECT(muteswitch), hmutesignalid);	
	}
}

void lamixer_volbox_changevalue(GtkWidget *volbar, glong value, gulong hsignalid)
{
		g_signal_handler_block(G_OBJECT(volbar), hsignalid);
		gtk_range_set_value(GTK_RANGE(volbar), value);
		g_signal_handler_unblock (G_OBJECT(volbar), hsignalid);
}

void lamixer_volbox_check(GtkWidget *volbar, gulong hsignalid, glong value, glong *svalue)
{
	if (gtk_range_get_value(GTK_RANGE(volbar)) != value) {
		lamixer_volbox_changevalue(volbar, value, hsignalid);
		*svalue = value;
	}
}

static int lamixer_volbox_change(snd_mixer_elem_t *elem, unsigned int mask)
{
	long rangevalue=0;
	int mutesw;
	MixerElem *mixer_elem;

	if (mask == SND_CTL_EVENT_MASK_REMOVE)
		return 0;

	if (mask & SND_CTL_EVENT_MASK_VALUE) {
		mixer_elem = snd_mixer_elem_get_callback_private(elem);
		if(mixer_elem->playback != NULL) {
			snd_mixer_selem_get_playback_volume(elem, SND_MIXER_SCHN_FRONT_LEFT, &rangevalue);
			lamixer_volbox_check(mixer_elem->playback->volbar, mixer_elem->playback->hsignalid, rangevalue, &mixer_elem->playback->curval);

			if(mixer_elem->playback->rvolbar != NULL)
			{
				rangevalue=0;
				snd_mixer_selem_get_playback_volume(elem, SND_MIXER_SCHN_FRONT_RIGHT, &rangevalue);
				lamixer_volbox_check(mixer_elem->playback->rvolbar, mixer_elem->playback->hsignalidr, rangevalue, &mixer_elem->playback->rcurval);
			}
			
			if(mixer_elem->playback->muteswitch != NULL) {
				snd_mixer_selem_get_playback_switch (elem, SND_MIXER_SCHN_FRONT_LEFT, &mutesw);
				lamixer_switch_checkvalue(mixer_elem->playback->muteswitch, mutesw, mixer_elem->playback->hmutesignalid);
			}
		}
		if(mixer_elem->capture != NULL) {
			rangevalue=0;
			snd_mixer_selem_get_capture_volume(elem, SND_MIXER_SCHN_FRONT_LEFT, &rangevalue);
			lamixer_volbox_check(mixer_elem->capture->volbar, mixer_elem->capture->hsignalid, rangevalue, &mixer_elem->capture->curval);
			if(mixer_elem->capture->rvolbar != NULL)
			{
				rangevalue=0;
				snd_mixer_selem_get_capture_volume(elem, SND_MIXER_SCHN_FRONT_RIGHT, &rangevalue);
				lamixer_volbox_check(mixer_elem->capture->rvolbar, mixer_elem->capture->hsignalidr, rangevalue, &mixer_elem->capture->rcurval);
			}
			
			if(mixer_elem->capture->muteswitch != NULL) {
				snd_mixer_selem_get_capture_switch (elem, SND_MIXER_SCHN_FRONT_LEFT, &mutesw);
				lamixer_switch_checkvalue(mixer_elem->capture->muteswitch, mutesw, mixer_elem->capture->hmutesignalid);
			}
		}
	}

	return 0;
}

static gboolean lamixer_volbox_alsa_io(GIOChannel *chan, GIOCondition condition, gpointer data)
{
	//FIXME: Do we realy need G_IO_ERR G_IO_HUP G_IO_NVAL ?
	switch(condition) {
		case G_IO_OUT:
		case G_IO_IN:
		case G_IO_PRI:
			snd_mixer_handle_events(mixer_handle);
			return TRUE;
		case G_IO_ERR:
		case G_IO_HUP:
		case G_IO_NVAL:
		default:
			g_warning("ALSA mixer polling error");
			return FALSE;
	}
}

void lamixer_volbox_changed(GtkObject *adjustment, VolBox *volumebox)
{
	long rangevalue_left,rangevalue_right,increment;

	rangevalue_left = gtk_range_get_value(GTK_RANGE(adjustment));
	if (volumebox->type == 1) {
		snd_mixer_selem_set_playback_volume (volumebox->volelem, SND_MIXER_SCHN_FRONT_LEFT, rangevalue_left);
	}
	else if (volumebox->type == 2) {
		snd_mixer_selem_set_capture_volume (volumebox->volelem, SND_MIXER_SCHN_FRONT_LEFT, rangevalue_left);
	}

	if ((volumebox->lockswitch != NULL) && (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(volumebox->lockswitch)))) {

		if (volumebox->type == 1) {
			snd_mixer_selem_get_playback_volume (volumebox->volelem, SND_MIXER_SCHN_FRONT_RIGHT, &rangevalue_right);
		}
		else if (volumebox->type == 2) {
			snd_mixer_selem_get_capture_volume (volumebox->volelem, SND_MIXER_SCHN_FRONT_RIGHT, &rangevalue_right);
		}

		//TODO: Percent increment will be better?
		increment = rangevalue_left-volumebox->curval;

		if ( ((rangevalue_right<volumebox->maxrange) && (rangevalue_right>volumebox->minrange)) || \
		    ((rangevalue_right==volumebox->maxrange) && (increment<0)) || ((rangevalue_right==volumebox->minrange) && (increment>0)) ) {		

				if (((rangevalue_right+increment)<=(long)volumebox->maxrange) && ((rangevalue_right+increment)>=(long)volumebox->minrange)) {
					rangevalue_right = rangevalue_right+increment;
				}
				else {
					if ((rangevalue_right+increment) < ((long)volumebox->minrange))
						rangevalue_right = volumebox->minrange;
					else
						rangevalue_right = volumebox->maxrange;
				}
				g_signal_handler_block(G_OBJECT(volumebox->rvolbar), volumebox->hsignalidr);
				gtk_range_set_value(GTK_RANGE(volumebox->rvolbar), rangevalue_right);
				if (volumebox->type == 1) {
					snd_mixer_selem_set_playback_volume (volumebox->volelem, SND_MIXER_SCHN_FRONT_RIGHT, rangevalue_right);
				}
				else if (volumebox->type == 2) {
					snd_mixer_selem_set_capture_volume (volumebox->volelem, SND_MIXER_SCHN_FRONT_RIGHT, rangevalue_right);
				}
				volumebox->rcurval = rangevalue_right;
				g_signal_handler_unblock(G_OBJECT(volumebox->rvolbar), volumebox->hsignalidr);
			}
	}
	volumebox->curval = rangevalue_left;
}

void lamixer_volboxr_changed(GtkObject *adjustment, VolBox *volumebox)
{
	long rangevalue_left,rangevalue_right,increment;

	rangevalue_right = gtk_range_get_value(GTK_RANGE(adjustment));

	if (volumebox->type == 1) {
		snd_mixer_selem_set_playback_volume (volumebox->volelem, SND_MIXER_SCHN_FRONT_RIGHT, rangevalue_right);
	}
	else if (volumebox->type == 2) {
		snd_mixer_selem_set_capture_volume (volumebox->volelem, SND_MIXER_SCHN_FRONT_RIGHT, rangevalue_right);
	}

	if ((volumebox->lockswitch != NULL) && (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(volumebox->lockswitch)))) {

		if (volumebox->type == 1) {
			snd_mixer_selem_get_playback_volume (volumebox->volelem, SND_MIXER_SCHN_FRONT_LEFT, &rangevalue_left);
		}
		else if (volumebox->type == 2) {
			snd_mixer_selem_get_capture_volume (volumebox->volelem, SND_MIXER_SCHN_FRONT_LEFT, &rangevalue_left);
		}

		//TODO: Percent increment will be better?
		increment = rangevalue_right-volumebox->rcurval;

		if ( ((rangevalue_left<volumebox->maxrange) && (rangevalue_left>volumebox->minrange)) || \
		    ((rangevalue_left==volumebox->maxrange) && (increment<0)) || ((rangevalue_left==volumebox->minrange) && (increment>0)) ) {

				if (((rangevalue_left+increment)<=(long)volumebox->maxrange) && ((rangevalue_left+increment)>=(long)volumebox->minrange)) {
					rangevalue_left = rangevalue_left+increment;
				}
				else {
					if ((rangevalue_left+increment) < ((long)volumebox->minrange)) {
						rangevalue_left = volumebox->minrange;
					}
					else {
						rangevalue_left = volumebox->maxrange;
					}
				}
				g_signal_handler_block(G_OBJECT(volumebox->volbar), volumebox->hsignalid);
				gtk_range_set_value(GTK_RANGE(volumebox->volbar), rangevalue_left);
				if (volumebox->type == 1) {
					snd_mixer_selem_set_playback_volume (volumebox->volelem, SND_MIXER_SCHN_FRONT_LEFT, rangevalue_left);
				}
				else if (volumebox->type == 2) {
					snd_mixer_selem_set_capture_volume (volumebox->volelem, SND_MIXER_SCHN_FRONT_LEFT, rangevalue_left);
				}
				volumebox->curval = rangevalue_left;
				g_signal_handler_unblock(G_OBJECT(volumebox->volbar), volumebox->hsignalid);
			}
	}
	volumebox->rcurval = rangevalue_right;
}

SwitchBox *lamixer_switchbox_add(const gchar *str, snd_mixer_elem_t *elem, guint voltype)
{
	SwitchBox *swbox;

	swbox = g_malloc(sizeof(SwitchBox));
	swbox->type = voltype;
	swbox->volelem = elem;
	swbox->volswitch = gtk_check_button_new_with_label(str);

	return swbox;
}

EnumBox *lamixer_enumbox_add(const gchar *str, snd_mixer_elem_t *elem, guint voltype)
{
	EnumBox *enumbox;

	enumbox = g_malloc(sizeof(EnumBox));
	enumbox->type = voltype;
	enumbox->volelem = elem;
	enumbox->enumhbox = gtk_hbox_new(FALSE,0);
	enumbox->enumname = gtk_label_new(str);
	enumbox->enumswitch = gtk_combo_box_new_text();

	int enumsCnt = snd_mixer_selem_get_enum_items(elem);
	for(int i=0; i<enumsCnt; i++) {
		char iname[256];
		snd_mixer_selem_get_enum_item_name(elem, i, sizeof(iname), iname);
		gtk_combo_box_append_text (GTK_COMBO_BOX (enumbox->enumswitch), iname);
	}
	
	gtk_box_pack_start(GTK_BOX(enumbox->enumhbox), enumbox->enumname, FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(enumbox->enumhbox), enumbox->enumswitch, FALSE, FALSE, 0);

	return enumbox;
}

VolBox *lamixer_volbox_new(const gchar *str, const gint mono, const gint mute, const glong minrange, const glong maxrange, snd_mixer_elem_t *elem, guint voltype)
{
	VolBox *volumebox;

	volumebox = g_malloc(sizeof(VolBox));
	volumebox->minrange = minrange;
	volumebox->maxrange = maxrange;
	volumebox->type = voltype;
	volumebox->volelem = elem;
	volumebox->volbox = gtk_vbox_new(FALSE,0);
	volumebox->volbarbox = gtk_hbox_new(FALSE,0);
	volumebox->vollabel = gtk_label_new(str);
	volumebox->volbar = gtk_vscale_new_with_range(minrange,maxrange,1.0);
	gtk_range_set_inverted(GTK_RANGE(volumebox->volbar), TRUE);
	gtk_scale_set_digits(GTK_SCALE(volumebox->volbar),0);
	gtk_scale_set_value_pos(GTK_SCALE(volumebox->volbar),GTK_POS_BOTTOM);
	//FIXME: Need any other method to set size
	gtk_widget_set_size_request(GTK_WIDGET(volumebox->volbar),0,200);
	if (!mono) {
		volumebox->rvolbar = gtk_vscale_new_with_range(minrange,maxrange,1.0);
		gtk_scale_set_digits(GTK_SCALE(volumebox->rvolbar),0);
		gtk_range_set_inverted(GTK_RANGE(volumebox->rvolbar), TRUE);
		gtk_scale_set_value_pos(GTK_SCALE(volumebox->rvolbar),GTK_POS_BOTTOM);
		gtk_widget_set_size_request(GTK_WIDGET(volumebox->rvolbar),0,200);
		volumebox->lockswitch = gtk_check_button_new_with_label("Lock");
	}
	else {
		volumebox->rvolbar = NULL;
		volumebox->lockswitch = NULL;
	}

	if (mute)
		volumebox->muteswitch = gtk_check_button_new_with_label("Mute");
	else
		volumebox->muteswitch = NULL;

	return volumebox;
}

MixerElem *lamixer_volelem_new(snd_mixer_elem_t *elem)
{
	MixerElem *mixer_elem;
	guint voltype=0, elemidx=0, mono;
	long minrange=0,maxrange=0,rangevalue_left=0,rangevalue_right=0;
	const char* selem_name = NULL;
	char elem_name[64];
	
	mixer_elem = g_malloc(sizeof(MixerElem));
	mixer_elem->elem = elem;
	mixer_elem->playback = NULL;
	mixer_elem->capture = NULL;
	mixer_elem->pswitch = NULL;
	mixer_elem->cswitch = NULL;
	mixer_elem->penum = NULL;
	mixer_elem->cenum = NULL;
	
	elemidx = snd_mixer_selem_get_index(elem);
	selem_name = snd_mixer_selem_id_get_name(sid);
	strcpy(elem_name, selem_name);
	if(elemidx > 0) {
		sprintf(elem_name,"%s %i",elem_name, elemidx);
	}
	
	if (!snd_mixer_selem_is_enumerated(elem)) {
		if (snd_mixer_selem_has_playback_volume(elem)) {
			voltype = 1;
			snd_mixer_selem_get_playback_volume_range (elem, &minrange, &maxrange);
			mono = snd_mixer_selem_is_playback_mono(elem);
			snd_mixer_selem_get_playback_volume (elem, SND_MIXER_SCHN_FRONT_LEFT, &rangevalue_left);
			
			if(!mono)
				snd_mixer_selem_get_playback_volume (elem, SND_MIXER_SCHN_FRONT_RIGHT, &rangevalue_right);
			
			mixer_elem->playback = lamixer_volbox_new(elem_name,mono, \
			                                                (snd_mixer_selem_has_playback_switch(elem) || snd_mixer_selem_has_playback_switch_joined(elem)), minrange, maxrange, elem, voltype);
		}
		else if (snd_mixer_selem_has_playback_switch(elem)) {
			voltype = 1;
			mixer_elem->pswitch = lamixer_switchbox_add(elem_name, elem, voltype);
		}
		
		if (snd_mixer_selem_has_capture_volume(elem)) {
			voltype = 2;
			snd_mixer_selem_get_capture_volume_range (elem, &minrange, &maxrange);
			mono = snd_mixer_selem_is_capture_mono(elem);
			snd_mixer_selem_get_capture_volume (elem, SND_MIXER_SCHN_FRONT_LEFT, &rangevalue_left);
			
			if(!mono)
				snd_mixer_selem_get_capture_volume (elem, SND_MIXER_SCHN_FRONT_RIGHT, &rangevalue_right);
			
			mixer_elem->capture = lamixer_volbox_new(elem_name,mono, \
									(snd_mixer_selem_has_capture_switch(elem) || snd_mixer_selem_has_capture_switch_joined(elem)), minrange, maxrange, elem, voltype);
		}
		else if (snd_mixer_selem_has_capture_switch(elem)) {
			voltype = 2;
			mixer_elem->cswitch = lamixer_switchbox_add(elem_name, elem, voltype);
		}
	}
	else {
		if (snd_mixer_selem_is_enum_playback(elem)) {
			voltype = 1;
			mixer_elem->penum = lamixer_enumbox_add(elem_name, elem, voltype);
		}
		else if (snd_mixer_selem_is_enum_capture(elem)) {
			voltype = 2;
			mixer_elem->cenum = lamixer_enumbox_add(elem_name, elem, voltype);
		}
	}
		
	return mixer_elem;
}

void lamixer_switchbox_show(SwitchBox *swbox, GtkWidget *swvbox)
{
	int swstat;

	snd_mixer_selem_get_playback_switch (swbox->volelem, SND_MIXER_SCHN_FRONT_LEFT, &swstat);

	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(swbox->volswitch), swstat);
	swbox->hswsignalid = g_signal_connect(G_OBJECT(swbox->volswitch), "toggled", G_CALLBACK(lamixer_volswitch_changed), swbox->volelem);
	gtk_box_pack_start(GTK_BOX(swvbox), swbox->volswitch, TRUE, TRUE, 0);

	snd_mixer_elem_set_callback(swbox->volelem, lamixer_switchbox_change);
	snd_mixer_elem_set_callback_private(swbox->volelem, swbox);
}

static int lamixer_enumbox_change(snd_mixer_elem_t *elem, unsigned int mask)
{
	if (mask == SND_CTL_EVENT_MASK_REMOVE)
		return 0;

	if (mask & SND_CTL_EVENT_MASK_VALUE) {
		guint evalue;
		EnumBox *enmbox = NULL;
		
		enmbox = snd_mixer_elem_get_callback_private(elem);
		snd_mixer_selem_get_enum_item(elem, SND_MIXER_SCHN_FRONT_LEFT, &evalue);
		
		gtk_combo_box_set_active(GTK_COMBO_BOX(enmbox->enumswitch), evalue);
	}

	return 0;
}

void lamixer_enumbox_changed(GtkObject *enumswitch, snd_mixer_elem_t *elem)
{
	snd_mixer_selem_set_enum_item(elem, SND_MIXER_SCHN_FRONT_LEFT,  gtk_combo_box_get_active(GTK_COMBO_BOX(enumswitch)));
}

void lamixer_enumbox_show(EnumBox *enumbox, GtkWidget *enumvbox)
{
	guint evalue;
	snd_mixer_selem_get_enum_item(enumbox->volelem, SND_MIXER_SCHN_FRONT_LEFT, &evalue);
	gtk_combo_box_set_active(GTK_COMBO_BOX(enumbox->enumswitch), evalue);
	gtk_box_pack_start(GTK_BOX(enumvbox), enumbox->enumhbox, TRUE, FALSE, 0);
	g_signal_connect(G_OBJECT(enumbox->enumswitch), "changed", G_CALLBACK(lamixer_enumbox_changed), enumbox->volelem);
	
	snd_mixer_elem_set_callback(enumbox->volelem, lamixer_enumbox_change);
	snd_mixer_elem_set_callback_private(enumbox->volelem, enumbox);
}

void lamixer_volbox_show(VolBox *volumebox, snd_mixer_elem_t *elem, GtkWidget *mixerbox)
{
	long rangevalue_left,rangevalue_right;
	int mute, *bCols;

	if (volumebox->type == 1) {
		snd_mixer_selem_get_playback_volume (elem, SND_MIXER_SCHN_FRONT_LEFT, &rangevalue_left);
		bCols = &pCols;
	}
	else if (volumebox->type == 2) {
		snd_mixer_selem_get_capture_volume (elem, SND_MIXER_SCHN_FRONT_LEFT, &rangevalue_left);
		bCols = &cCols;
	}

	volumebox->curval = rangevalue_left;
	
	gtk_table_attach(GTK_TABLE(mixerbox), volumebox->vollabel, *bCols-1, *bCols, 0, 1, GTK_SHRINK, GTK_SHRINK, 0, 0);
	
	gtk_range_set_value(GTK_RANGE(volumebox->volbar), rangevalue_left);
	gtk_box_pack_start(GTK_BOX(volumebox->volbarbox), volumebox->volbar, TRUE, TRUE, 0);

	volumebox->hsignalid = g_signal_connect(G_OBJECT(volumebox->volbar), "value-changed", G_CALLBACK(lamixer_volbox_changed), volumebox);

	if (volumebox->rvolbar != NULL) {
		if (volumebox->type == 1) {
			snd_mixer_selem_get_playback_volume (elem, SND_MIXER_SCHN_FRONT_RIGHT, &rangevalue_right);
		}
		else if (volumebox->type == 2) {
			snd_mixer_selem_get_capture_volume (elem, SND_MIXER_SCHN_FRONT_RIGHT, &rangevalue_right);
		}

		volumebox->rcurval = rangevalue_right;

		gtk_box_pack_start(GTK_BOX(volumebox->volbarbox), volumebox->rvolbar, TRUE, TRUE, 0);
		gtk_range_set_value(GTK_RANGE(volumebox->rvolbar), rangevalue_right);

		volumebox->hsignalidr = g_signal_connect(G_OBJECT(volumebox->rvolbar), "value-changed", G_CALLBACK(lamixer_volboxr_changed), volumebox);
	}

	gtk_table_attach(GTK_TABLE(mixerbox), volumebox->volbarbox, *bCols-1, *bCols, 1, 2, GTK_FILL | GTK_SHRINK, GTK_FILL | GTK_EXPAND | GTK_SHRINK, 0, 0);

	if (volumebox->lockswitch != NULL) {
		gtk_table_attach(GTK_TABLE(mixerbox), volumebox->lockswitch, *bCols-1, *bCols, 2, 3, GTK_FILL | GTK_SHRINK, GTK_FILL | GTK_SHRINK, 0, 0);
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(volumebox->lockswitch), TRUE);
	}

	if (volumebox->muteswitch != NULL) {
		snd_mixer_selem_get_playback_switch (elem, SND_MIXER_SCHN_FRONT_LEFT, &mute);
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(volumebox->muteswitch), !mute);
		volumebox->hmutesignalid = g_signal_connect(G_OBJECT(volumebox->muteswitch), "toggled", G_CALLBACK(lamixer_muteswitch_changed), elem);
		gtk_table_attach(GTK_TABLE(mixerbox), volumebox->muteswitch, *bCols-1, *bCols, 3, 4, GTK_SHRINK, GTK_SHRINK, 0, 0);
	}
	
	gtk_table_resize(GTK_TABLE(mixerbox), mRows, ++(*bCols));
	gtk_table_attach(GTK_TABLE(mixerbox), gtk_vseparator_new (), *bCols-1, *bCols, 0, 5, GTK_FILL | GTK_SHRINK, GTK_FILL | GTK_SHRINK, 4, 0);
	gtk_table_resize(GTK_TABLE(mixerbox), mRows, ++(*bCols));
}

void lamixer_mixerelem_show(MixerElem *mixer_elem)
{
	if(mixer_elem->playback != NULL)
		lamixer_volbox_show(mixer_elem->playback, mixer_elem->elem, mixerbox);
	
	if(mixer_elem->capture != NULL)
		lamixer_volbox_show(mixer_elem->capture, mixer_elem->elem, capturebox);
	
	if(mixer_elem->pswitch != NULL)
		lamixer_switchbox_show(mixer_elem->pswitch, switchvbox);
	
	if(mixer_elem->cswitch != NULL)
		lamixer_switchbox_show(mixer_elem->cswitch, switchcapturevbox);
	
	if(mixer_elem->penum != NULL)
		lamixer_enumbox_show(mixer_elem->penum, enumvbox);
	
	if(mixer_elem->cenum != NULL)
		lamixer_enumbox_show(mixer_elem->cenum, enumcapturevbox);
	
	if(mixer_elem->playback != NULL || mixer_elem->capture != NULL) {
		snd_mixer_elem_set_callback(mixer_elem->elem, lamixer_volbox_change);
		snd_mixer_elem_set_callback_private(mixer_elem->elem, mixer_elem);
	}
}

void lamixer_enum_cards()
{
	int card_index = -1;

	while(!snd_card_next(&card_index))
	{
		if(card_index > -1) {
			char* card_name;
			if(!snd_card_get_name(card_index, &card_name)) {
				SoundCards = g_list_append(SoundCards, card_name);
			}
		}
		else
			break;
	}
}

void lamixer_mixer_init(char card[64])
{
	snd_mixer_elem_t *elem;
	gint err;
	
	snd_mixer_selem_id_alloca(&sid);

	err = snd_mixer_open (&mixer_handle, 0);
	err = snd_mixer_attach (mixer_handle, card);
	err = snd_mixer_selem_register (mixer_handle, NULL, NULL);
	err = snd_mixer_load (mixer_handle);
	
	for (elem = snd_mixer_first_elem(mixer_handle); elem; elem = snd_mixer_elem_next(elem)) {
		snd_mixer_selem_get_id(elem, sid);

		if (!snd_mixer_selem_is_active(elem))
			continue;

		VolumeBoxes = g_list_append(VolumeBoxes, lamixer_volelem_new(elem));
	}
}

void lamixer_mixer_elemets()
{
	gint cnt = 0, i;
	struct pollfd *fds;

	g_list_foreach (VolumeBoxes, (GFunc)lamixer_mixerelem_show, NULL);
	
	gtk_table_resize(GTK_TABLE(mixerbox), mRows, ++pCols);
	gtk_table_attach(GTK_TABLE(mixerbox), switchvbox, pCols-1, pCols, 0, 5, GTK_FILL | GTK_EXPAND | GTK_SHRINK, GTK_FILL | GTK_EXPAND | GTK_SHRINK, 0, 0);
	
	gtk_table_resize(GTK_TABLE(capturebox), mRows, ++cCols);
	gtk_table_attach(GTK_TABLE(capturebox), switchcapturevbox, cCols-1, cCols, 0, 5, GTK_FILL | GTK_EXPAND | GTK_SHRINK, GTK_FILL | GTK_EXPAND | GTK_SHRINK, 0, 0);
	
	cnt = snd_mixer_poll_descriptors_count(mixer_handle);

	fds = g_new0(struct pollfd, cnt);

	snd_mixer_poll_descriptors(mixer_handle, fds, cnt);

	alsa_chans = g_new0(GIOChannel*, cnt);

	for (i=0; i<cnt; i++) {
		alsa_chans[i] = g_io_channel_unix_new(fds[i].fd);
		g_io_add_watch(alsa_chans[i], G_IO_IN | G_IO_OUT | G_IO_PRI | G_IO_ERR | G_IO_HUP | G_IO_NVAL, lamixer_volbox_alsa_io, NULL);
	}
	
	g_free(fds);
	n_alsa_chans = cnt;
}

void lamixer_widgets_destroy(GtkWidget *container)
{
	GList *elems = gtk_container_get_children(GTK_CONTAINER(container));
	g_list_foreach(elems, (GFunc)gtk_widget_destroy, NULL);
	g_list_free(elems);
}

void lamixer_mixer_destroy()
{
	g_free(alsa_chans);
	snd_mixer_detach(mixer_handle, card_id);
	snd_mixer_close(mixer_handle);
	
	lamixer_widgets_destroy(mixerbox);
	lamixer_widgets_destroy(capturebox);

	g_list_free (VolumeBoxes);
	VolumeBoxes = NULL;
}

void InitMixer(char card[64])
{
	switchvbox = gtk_vbox_new(FALSE,0);
	switchcapturevbox = gtk_vbox_new(FALSE,0);

	enumvbox = gtk_vbox_new(FALSE,0);
	enumcapturevbox = gtk_vbox_new(FALSE,0);
	
	gtk_box_pack_end(GTK_BOX(switchvbox), enumvbox, TRUE, TRUE, 0);
	gtk_box_pack_end(GTK_BOX(switchcapturevbox), enumcapturevbox, TRUE, TRUE, 0);
	
	pCols = cCols = 1;
	gtk_table_resize(GTK_TABLE(mixerbox), mRows, pCols);
	gtk_table_resize(GTK_TABLE(capturebox), mRows, cCols);

	lamixer_mixer_elemets();
	gtk_widget_show_all (mixerbox);
	gtk_widget_show_all (capturebox);
}

void lamixer_mixer_switch()
{
	char card[64];
	
	sprintf(card,"hw:%i",gtk_combo_box_get_active (GTK_COMBO_BOX(switchmixer)));
	lamixer_mixer_destroy();
	lamixer_mixer_init(card);
	InitMixer(card);
	lamixer_window_resize();
}

void InitScreen(void)
{
	GtkWidget *notebook;
	GtkWidget *playbacklabel;
	GtkWidget *capturelabel;

	window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
	gtk_window_set_title (GTK_WINDOW(window), "Lightweight Alsa Mixer");
	//gtk_container_set_border_width (GTK_CONTAINER (window), 1);
	gtk_signal_connect (GTK_OBJECT(window), "destroy", GTK_SIGNAL_FUNC(gtk_main_quit), NULL);
	
	windowbox = gtk_vbox_new(FALSE,1);
	
	InitMenu();
	gtk_box_pack_start(GTK_BOX(windowbox), menu_bar, FALSE, TRUE, 0);
	
	lamixer_enum_cards();
	lamixer_mixer_init(card_id);
	mixerbox = gtk_table_new(mRows, 1, FALSE);
	capturebox = gtk_table_new(mRows, 1, FALSE);

	InitMixer(card_id);
	
	playbackscrollwin = gtk_scrolled_window_new (NULL, NULL);
	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (playbackscrollwin),GTK_POLICY_AUTOMATIC, GTK_POLICY_NEVER);
	gtk_scrolled_window_add_with_viewport(GTK_SCROLLED_WINDOW (playbackscrollwin), mixerbox);

	capturescrollwin = gtk_scrolled_window_new (NULL, NULL);
	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (capturescrollwin),GTK_POLICY_AUTOMATIC, GTK_POLICY_NEVER);
	gtk_scrolled_window_add_with_viewport(GTK_SCROLLED_WINDOW (capturescrollwin), capturebox);

	playbacklabel =  gtk_label_new("Playback");
	capturelabel =  gtk_label_new("Capture");
	
	notebook = gtk_notebook_new ();
	gtk_notebook_set_tab_pos (GTK_NOTEBOOK (notebook), GTK_POS_TOP);
	gtk_notebook_append_page (GTK_NOTEBOOK (notebook), playbackscrollwin, playbacklabel);
	gtk_notebook_append_page (GTK_NOTEBOOK (notebook), capturescrollwin, capturelabel);
	
	switchmixer = gtk_combo_box_new_text();
	
	GList *card_name = g_list_first(SoundCards);	
	while(card_name) {
		gtk_combo_box_append_text (GTK_COMBO_BOX (switchmixer), card_name->data);
		card_name = g_list_next(card_name);
	}
	
	gtk_combo_box_set_active(GTK_COMBO_BOX (switchmixer), 0);
	g_signal_connect(G_OBJECT(switchmixer), "changed", G_CALLBACK(lamixer_mixer_switch), NULL);
	
	gtk_notebook_set_action_widget (GTK_NOTEBOOK (notebook), switchmixer, GTK_PACK_END);
	gtk_widget_show(switchmixer);

	gtk_box_pack_start(GTK_BOX(windowbox), notebook, TRUE, TRUE, 0);
	gtk_container_add (GTK_CONTAINER(window), windowbox);	
	lamixer_window_resize();
	
    gtk_widget_show_all (window);
}

void InitMenu()
{
    //menu = gtk_menu_new ();
	
	menu_mixer = gtk_menu_item_new_with_label ("Mixer");
	menu_view = gtk_menu_item_new_with_label ("View");
	menu_help = gtk_menu_item_new_with_label ("Help");
	
	//gtk_menu_item_set_submenu (GTK_MENU_ITEM (menu_view), menu);
	//gtk_menu_item_set_submenu (GTK_MENU_ITEM (menu_mixer), menu);
	//gtk_menu_item_set_submenu (GTK_MENU_ITEM (menu_help), menu);
	
	menu_bar = gtk_menu_bar_new ();
	
	gtk_menu_shell_append (GTK_MENU_SHELL (menu_bar), menu_mixer);
	gtk_menu_shell_append (GTK_MENU_SHELL (menu_bar), menu_view);
	gtk_menu_shell_append (GTK_MENU_SHELL (menu_bar), menu_help);
}

void lamixer_window_resize()
{
    GdkScreen* screen = NULL;
    int screen_width, window_width;

	screen = gtk_window_get_screen(GTK_WINDOW(window));
	screen_width = gdk_screen_get_width(screen);
    
    GtkRequisition *requisition = g_malloc(sizeof(GtkRequisition));
    gtk_widget_size_request(mixerbox,requisition);
    
    if(requisition->width < screen_width)
		window_width = requisition->width+25;
	else
		window_width = screen_width * 2 / 3;
	
	int widow_height = requisition->height;
	
	gtk_widget_size_request(window,requisition);
	gtk_window_resize (GTK_WINDOW(window), window_width, widow_height);
}

int main( int   argc,
         char *argv[] )
{
	gtk_init (&argc, &argv);
	InitScreen();
	gtk_main ();

	return 0;
}

//TODO: menu (Mixer->(Settings, Exit), View->(Simple,Advanced), Help->About)
//TODO: AboutDlg
//TODO: SettingsDlg(default view, default hw, simple view set)
//TODO: custom checklistbox for view set with icons per item chose
//TODO: simple view(get set, replace header with icon, replace lock with icon, replace mute with green radiobuton)
//Final!
