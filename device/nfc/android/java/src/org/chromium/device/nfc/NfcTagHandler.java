// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.device.nfc;

import android.nfc.FormatException;
import android.nfc.NdefMessage;
import android.nfc.Tag;
import android.nfc.TagLostException;
import android.nfc.tech.Ndef;
import android.nfc.tech.NdefFormatable;
import android.nfc.tech.TagTechnology;

import java.io.IOException;

/**
 * Utility class that provides I/O operations for NFC tags.
 */
public final class NfcTagHandler {
    private final TagTechnology mTech;
    private final TagTechnologyHandler mTechHandler;
    private boolean mWasConnected;

    /**
     * Factory method that creates NfcTagHandler for a given NFC Tag.
     *
     * @param tag @see android.nfc.Tag
     * @return NfcTagHandler or null when unsupported Tag is provided.
     */
    public static NfcTagHandler create(Tag tag) {
        if (tag == null) return null;

        Ndef ndef = Ndef.get(tag);
        if (ndef != null) return new NfcTagHandler(ndef, new NdefHandler(ndef));

        NdefFormatable formattable = NdefFormatable.get(tag);
        if (formattable != null) {
            return new NfcTagHandler(formattable, new NdefFormattableHandler(formattable));
        }

        return null;
    }

    /**
     * NdefFormatable and Ndef interfaces have different signatures for operating with NFC tags.
     * This interface provides generic methods.
     */
    private interface TagTechnologyHandler {
        public void write(NdefMessage message)
                throws IOException, TagLostException, FormatException;
        // TODO(crbug.com/625589): add read method for nfc.watch.
    }

    /**
     * Implementation of TagTechnologyHandler that uses Ndef tag technology.
     * @see android.nfc.tech.Ndef
     */
    private static class NdefHandler implements TagTechnologyHandler {
        private final Ndef mNdef;

        NdefHandler(Ndef ndef) {
            mNdef = ndef;
        }

        public void write(NdefMessage message)
                throws IOException, TagLostException, FormatException {
            mNdef.writeNdefMessage(message);
        }
    }

    /**
     * Implementation of TagTechnologyHandler that uses NdefFormatable tag technology.
     * @see android.nfc.tech.NdefFormatable
     */
    private static class NdefFormattableHandler implements TagTechnologyHandler {
        private final NdefFormatable mNdefFormattable;

        NdefFormattableHandler(NdefFormatable ndefFormattable) {
            mNdefFormattable = ndefFormattable;
        }

        public void write(NdefMessage message)
                throws IOException, TagLostException, FormatException {
            mNdefFormattable.format(message);
        }
    }

    private NfcTagHandler(TagTechnology tech, TagTechnologyHandler handler) {
        mTech = tech;
        mTechHandler = handler;
    }

    /**
     * Connects to NFC tag.
     */
    public void connect() throws IOException, TagLostException {
        if (!mTech.isConnected()) {
            mTech.connect();
            mWasConnected = true;
        }
    }

    /**
     * Closes connection.
     */
    public void close() throws IOException {
        mTech.close();
    }

    /**
     * Writes NdefMessage to NFC tag.
     */
    public void write(NdefMessage message) throws IOException, TagLostException, FormatException {
        mTechHandler.write(message);
    }

    /**
     * If tag was previously connected and subsequent connection to the same tag fails, consider
     * tag to be out of range.
     */
    public boolean isTagOutOfRange() {
        try {
            connect();
        } catch (IOException e) {
            return mWasConnected;
        }
        return false;
    }
}